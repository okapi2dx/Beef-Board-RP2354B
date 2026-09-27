#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "beef_runtime.h"
#include "beef_lighting.h"
#include "phoenixwan_uart.h"
#include "ws2812.pio.h"
#include "tusb.h"

#include "beef_config_store.h"
#include "beef_protocol.h"

#define CFG_DISABLE_LEDS              5u
#define CFG_IIDX_INPUT_MODE          32u
#define CFG_IIDX_BUTTONS_DEBOUNCE    75u
#define CFG_LED_REFRESH              78u
#define CFG_TT_LEDS                  80u
#define CFG_DIGITAL_TT               85u
#define CFG_BUTTON_LED_BRIGHTNESS    88u
#define CFG_BUTTON_LED_INVERT        89u
#define CFG_BUTTON_LED_INDIVIDUAL    90u
#define CFG_BUTTON_LED_FADE_BASE      91u
#define CFG_BUTTON_LED_LEVEL_BASE    102u
#define CFG_BUTTON_LED_INVERT_BASE   113u
#define CFG_BUTTON_LED_FADE_EXT_LO   125u
#define CFG_BUTTON_MAPPING_BASE      127u
#define CFG_BAR_LEDS                 138u

#define INPUT_MODE_JOYSTICK 0u
#define INPUT_MODE_KEYBOARD 1u


#define BUTTON_COUNT 11u

typedef unsigned int gpio_pin_t;

typedef struct __attribute__((packed)) {
  uint8_t x;
  uint8_t y;
  uint16_t buttons;
} gamepad_payload_t;

typedef struct __attribute__((packed)) {
  uint16_t buttons;
  uint8_t sensor_ab;
  uint8_t raw_position;
  uint8_t filtered_position;
  uint8_t output_position;
  int8_t direction;
  uint32_t transitions;
  uint16_t last_transition_ms;
  uint8_t led_levels[BUTTON_COUNT];
} diagnostics_t;

_Static_assert(sizeof(diagnostics_t) == BEEF_DIAGNOSTICS_SIZE,
               "diagnostics layout must remain 24 bytes");

static gpio_pin_t const button_inputs[BUTTON_COUNT] = {
  46, 44, 43, 36, 34, 32, 31, 7, 6, 24, 17
};
static gpio_pin_t const button_leds[BUTTON_COUNT] = {
  47, 45, 33, 35, 42, 30, 29, 4, 5, 26, 25
};
static PIO rgb_pio = pio0;
static uint rgb_sm_tt;
static uint rgb_sm_bar;
static repeating_timer_t led_timer;

static uint16_t raw_buttons;
static uint16_t stable_buttons;
static uint16_t logical_buttons;
static uint16_t previous_raw_buttons;
static uint32_t button_changed_ms[BUTTON_COUNT];

static volatile uint8_t encoder_prev;
static volatile int32_t encoder_delta;
static volatile uint32_t encoder_transitions;
static volatile uint32_t encoder_last_ms, encoder_interval_ms;
static beef_turntable_t turntable;
static uint16_t pending_tt_buttons;
static beef_lighting_t lighting;
static beef_rgb_t ring_pixels[BEEF_MAX_LEDS], bar_pixels[BEEF_MAX_LEDS];
static uint32_t ring_words[BEEF_MAX_LEDS], bar_words[BEEF_MAX_LEDS];
static int dma_tt, dma_bar;
static uint8_t previous_tt_count, previous_bar_count;
static uint32_t rgb_last_frame, rgb_dma_done_us;
static bool rgb_transmitting, rgb_latching;
static int8_t uart_tt_direction;
static uint32_t uart_tt_motion_ms;
static uint16_t uart_tt_interval_ms;

static diagnostics_t diagnostics;
static uint8_t previous_input_mode = 0xffu;

static uint16_t host_led_buttons;
static uint8_t host_tt_rgb[3];
static uint8_t host_center_rgb[3];
static uint32_t host_lighting_ms;
static bool host_lighting_seen;

static volatile uint8_t led_pwm_duty[BUTTON_COUNT];
static volatile uint8_t led_pwm_phase;

static bool led_active[BUTTON_COUNT];
static bool led_fading[BUTTON_COUNT];
static uint32_t led_released_ms[BUTTON_COUNT];

static void gpio_input_pullup(gpio_pin_t pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
}
static void gpio_output(gpio_pin_t pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  gpio_put(pin, 0);
}

static uint16_t read_le16(uint8_t const *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint ws2812_sm_init(uint pin) {
  uint sm = pio_claim_unused_sm(rgb_pio, true);
  static int program_offset = -1;
  if (program_offset < 0) program_offset = (int)pio_add_program(rgb_pio, &ws2812_program);
  pio_sm_config c = ws2812_program_get_default_config((uint)program_offset);
  sm_config_set_sideset_pins(&c, pin);
  sm_config_set_out_shift(&c, false, true, 24);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (800000.0f * 10.0f));
  pio_gpio_init(rgb_pio, pin);
  pio_sm_set_consecutive_pindirs(rgb_pio, sm, pin, 1, true);
  pio_sm_init(rgb_pio, sm, (uint)program_offset, &c);
  pio_sm_set_enabled(rgb_pio, sm, true);
  return sm;
}
static int ws2812_dma_init(uint sm) {
  int channel = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(channel);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  channel_config_set_dreq(&c, pio_get_dreq(rgb_pio, sm, true));
  dma_channel_configure(channel, &c, &rgb_pio->txf[sm], NULL, 0, false);
  return channel;
}
static uint32_t rgb_word(beef_rgb_t c) {
  return ((uint32_t)c.g<<24) | ((uint32_t)c.r<<16) | ((uint32_t)c.b<<8);
}
static void update_rgb_leds(uint32_t now) {
  if (rgb_transmitting) {
    if (dma_channel_is_busy(dma_tt) || dma_channel_is_busy(dma_bar)) return;
    rgb_transmitting = false; rgb_latching = true; rgb_dma_done_us = time_us_32();
  }
  // DMA completion precedes the final FIFO/shifter bits. Allow their 270 us
  // plus the WS2812 reset low time, without blocking input scanning or USB.
  if (rgb_latching && (uint32_t)(time_us_32()-rgb_dma_done_us)<400) return;
  rgb_latching = false;
  const uint8_t *cfg = beef_config_store_data();
  if (!beef_lighting_due(now, rgb_last_frame, cfg[CFG_LED_REFRESH])) return;
  rgb_last_frame = now;
  bool host_active = host_lighting_seen && (uint32_t)(now-host_lighting_ms)<1000;
  beef_lighting_render(&lighting,cfg,host_active,host_tt_rgb,host_center_rgb,ring_pixels,bar_pixels);
  // Send black pixels to the old tail when counts shrink.
  uint n=cfg[CFG_TT_LEDS]>previous_tt_count?cfg[CFG_TT_LEDS]:previous_tt_count;
  uint m=cfg[CFG_BAR_LEDS]>previous_bar_count?cfg[CFG_BAR_LEDS]:previous_bar_count;
  for(uint i=0;i<n;++i)ring_words[i]=rgb_word(ring_pixels[i]);
  for(uint i=0;i<m;++i)bar_words[i]=rgb_word(bar_pixels[i]);
  previous_tt_count=cfg[CFG_TT_LEDS]; previous_bar_count=cfg[CFG_BAR_LEDS];
  dma_channel_set_read_addr(dma_tt,ring_words,false);
  dma_channel_set_read_addr(dma_bar,bar_words,false);
  dma_channel_set_trans_count(dma_tt,n,false);
  dma_channel_set_trans_count(dma_bar,m,false);
  dma_start_channel_mask((1u<<dma_tt)|(1u<<dma_bar));
  rgb_transmitting=true;
}
static void encoder_irq(uint gpio, uint32_t events) {
  (void)gpio; (void)events;
  static const int8_t table[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  uint8_t current=(gpio_get(21)?1u:0u)|(gpio_get(20)?2u:0u);
  int8_t direction=table[(encoder_prev<<2)|current]; encoder_prev=current;
  if(direction) {
    encoder_delta+=direction; ++encoder_transitions;
    uint32_t now=to_ms_since_boot(get_absolute_time());
    encoder_interval_ms=now-encoder_last_ms; encoder_last_ms=now;
  }
}

static bool led_pwm_tick(repeating_timer_t *timer) {
  (void)timer;
  uint8_t phase = (uint8_t)(led_pwm_phase + 1u);
  if (phase >= 100u) phase = 0u;
  led_pwm_phase = phase;
  for (uint32_t i = 0; i < BUTTON_COUNT; ++i)
    gpio_put(button_leds[i], phase < led_pwm_duty[i]);
  return true;
}
static void controller_hardware_init(void) {
  for (uint32_t i = 0; i < BUTTON_COUNT; ++i) {
    gpio_input_pullup(button_inputs[i]);
    gpio_output(button_leds[i]);
  }
  gpio_input_pullup(21);
  gpio_input_pullup(20);
  gpio_input_pullup(3);
  gpio_input_pullup(2);
  rgb_sm_tt = ws2812_sm_init(1);
  rgb_sm_bar = ws2812_sm_init(0);
  dma_tt=ws2812_dma_init(rgb_sm_tt);
  dma_bar=ws2812_dma_init(rgb_sm_bar);
  uart_init(uart0, 115200);
  gpio_set_function(18, GPIO_FUNC_UART);
  gpio_set_function(19, GPIO_FUNC_UART);
  uart_init(uart1, 115200);
  gpio_set_function(22, GPIO_FUNC_UART);
  gpio_set_function(23, GPIO_FUNC_UART);
  phoenixwan_uart_init();
  encoder_prev = (gpio_get(21) ? 1u : 0u) | (gpio_get(20) ? 2u : 0u);
  gpio_set_irq_enabled_with_callback(21, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, encoder_irq);
  gpio_set_irq_enabled(20, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  add_repeating_timer_us(-100, led_pwm_tick, NULL, &led_timer);
}

static uint16_t sample_raw_buttons(void) {
  uint16_t state = 0;
  for (uint32_t i = 0; i < BUTTON_COUNT; ++i) {
    if (gpio_get(button_inputs[i]) == 0) {
      state |= (uint16_t)(1u << i);
    }
  }
  return state;
}

static void update_button_debounce(uint32_t now) {
  uint8_t const *cfg = beef_config_store_data();
  uint32_t debounce_ms = cfg[CFG_IIDX_BUTTONS_DEBOUNCE];

  raw_buttons = sample_raw_buttons();

  for (uint32_t i = 0; i < BUTTON_COUNT; ++i) {
    uint16_t mask = (uint16_t)(1u << i);
    bool raw = (raw_buttons & mask) != 0;
    bool previous_raw = (previous_raw_buttons & mask) != 0;
    bool stable = (stable_buttons & mask) != 0;

    if (raw != previous_raw) {
      button_changed_ms[i] = now;
      if (raw) previous_raw_buttons |= mask;
      else previous_raw_buttons &= (uint16_t)~mask;
    }

    if (raw != stable && (debounce_ms == 0u ||
        (uint32_t)(now - button_changed_ms[i]) >= debounce_ms)) {
      if (raw) stable_buttons |= mask;
      else stable_buttons &= (uint16_t)~mask;
    }
  }
}

static uint16_t remap_buttons(uint16_t physical) {
  uint8_t const *cfg = beef_config_store_data();
  uint16_t mapped = 0;

  for (uint32_t i = 0; i < BUTTON_COUNT; ++i) {
    if ((physical & (1u << i)) == 0u) continue;
    uint8_t logical = cfg[CFG_BUTTON_MAPPING_BASE + i];
    if (logical < BUTTON_COUNT) mapped |= (uint16_t)(1u << logical);
  }

  return mapped;
}

static void process_turntable(uint32_t now) {
  uint32_t irq=save_and_disable_interrupts();
  int32_t delta=encoder_delta; encoder_delta=0;
  restore_interrupts(irq);
  const uint8_t *cfg=beef_config_store_data();
  if (delta) {
    uint32_t interval = now - uart_tt_motion_ms;
    uart_tt_interval_ms = interval > 65535u ? 65535u : (uint16_t)interval;
    uart_tt_motion_ms = now;
    uart_tt_direction = (delta > 0) != (cfg[1] != 0) ? 1 : -1;
  }
  beef_turntable_update(&turntable,cfg,delta,now);
  if(cfg[CFG_DIGITAL_TT]) {
    if(turntable.output.direction<0)pending_tt_buttons=1u<<11;
    if(turntable.output.direction>0)pending_tt_buttons=1u<<12;
  } else pending_tt_buttons=0;
}

static uint16_t controller_buttons(void) {
  uint16_t result = remap_buttons(stable_buttons);
  uint8_t const *cfg = beef_config_store_data();

  if (cfg[CFG_DIGITAL_TT]) {
    if (turntable.output.direction < 0) result |= (uint16_t)(1u << 11);
    if (turntable.output.direction > 0) result |= (uint16_t)(1u << 12);
  }

  return result;
}

static void update_button_leds(uint32_t now) {
  uint8_t const *cfg = beef_config_store_data();
  bool individual = cfg[CFG_BUTTON_LED_INDIVIDUAL] != 0u;
  uint16_t global_fade = read_le16(&cfg[CFG_BUTTON_LED_FADE_EXT_LO]);
  bool host_active = host_lighting_seen &&
      (uint32_t)(now - host_lighting_ms) < 1000u;
  uint16_t led_source = host_active ? host_led_buttons : stable_buttons;

  for (uint32_t i = 0; i < BUTTON_COUNT; ++i) {
    bool on = (led_source & (1u << i)) != 0;

    bool inverted = cfg[CFG_BUTTON_LED_INVERT] != 0u;
    if (individual) {
      inverted ^= cfg[CFG_BUTTON_LED_INVERT_BASE + i] != 0u;
    }
    on ^= inverted;

    uint8_t brightness = individual
        ? cfg[CFG_BUTTON_LED_LEVEL_BASE + i]
        : cfg[CFG_BUTTON_LED_BRIGHTNESS];
    if (brightness > 100u) brightness = 100u;

    uint16_t fade = individual
        ? cfg[CFG_BUTTON_LED_FADE_BASE + i]
        : global_fade;

    uint8_t duty = 0u;

    if (cfg[CFG_DISABLE_LEDS] || brightness == 0u) {
      led_active[i] = false;
      led_fading[i] = false;
      duty = 0u;
    } else if (on) {
      led_active[i] = true;
      led_fading[i] = false;
      duty = brightness;
    } else {
      if (led_active[i]) {
        led_active[i] = false;
        led_fading[i] = true;
        led_released_ms[i] = now;
      }

      uint32_t elapsed = now - led_released_ms[i];
      if (!led_fading[i] || fade == 0u || elapsed >= fade) {
        led_fading[i] = false;
        duty = 0u;
      } else {
        duty = (uint8_t)(((uint32_t)brightness * (fade - elapsed)) / fade);
      }
    }

    led_pwm_duty[i] = duty;
  }
}

static void update_diagnostics(void) {
  diagnostics.buttons = stable_buttons; // Physical bits; Configurator applies mapping once.
  diagnostics.sensor_ab =
      (gpio_get(21) ? 1u : 0u) |
      (gpio_get(20) ? 2u : 0u);
  diagnostics.raw_position = turntable.raw;
  diagnostics.filtered_position = turntable.filtered;
  diagnostics.output_position = turntable.output.position;
  diagnostics.direction = turntable.output.direction;
  diagnostics.transitions = encoder_transitions;
  diagnostics.last_transition_ms = encoder_interval_ms > 65535 ? 65535 : (uint16_t)encoder_interval_ms;

  for (uint32_t i = 0; i < BUTTON_COUNT; ++i) {
    diagnostics.led_levels[i] = led_pwm_duty[i];
  }

  beef_protocol_set_diagnostics((uint8_t const *)&diagnostics);
}

static void send_controller_report(void) {
  if (!tud_mounted()) return;

  uint8_t const *cfg = beef_config_store_data();
  uint8_t mode = cfg[CFG_IIDX_INPUT_MODE];

  if (mode != previous_input_mode) {
    bool released = true;

    if (previous_input_mode == INPUT_MODE_KEYBOARD) {
      if (!tud_hid_n_ready(BEEF_HID_INSTANCE_KEYBOARD)) {
        released = false;
      } else {
        uint8_t released_keys[13] = {0};
        released = tud_hid_n_report(BEEF_HID_INSTANCE_KEYBOARD, 0,
                                    released_keys, sizeof(released_keys));
      }
    } else if (previous_input_mode == INPUT_MODE_JOYSTICK) {
      if (!tud_hid_n_ready(BEEF_HID_INSTANCE_GAMEPAD)) {
        released = false;
      } else {
        gamepad_payload_t released_gamepad = {.x = 127, .y = 127, .buttons = 0};
        released = tud_hid_n_report(BEEF_HID_INSTANCE_GAMEPAD, 0,
                                    &released_gamepad, sizeof(released_gamepad));
      }
    }

    if (!released) return;
    previous_input_mode = mode;
  }

  if (mode == INPUT_MODE_KEYBOARD) {
    uint8_t keys[13];
    beef_keyboard_report(cfg, logical_buttons | pending_tt_buttons, keys);

    if (tud_hid_n_ready(BEEF_HID_INSTANCE_KEYBOARD)) {
      if (tud_hid_n_report(BEEF_HID_INSTANCE_KEYBOARD, 0, keys, sizeof(keys))) pending_tt_buttons=0;
    }
  } else {
    uint16_t lower = logical_buttons & 0x007fu;
    uint16_t upper = (logical_buttons | pending_tt_buttons) >> 7;
    uint16_t iidx_buttons = lower | (uint16_t)(upper << 8);

    gamepad_payload_t report = {
      .x = cfg[CFG_DIGITAL_TT] ? 127u : turntable.output.position,
      .y = 127u,
      .buttons = iidx_buttons
    };
    if (tud_hid_n_ready(BEEF_HID_INSTANCE_GAMEPAD)) {
      if (tud_hid_n_report(BEEF_HID_INSTANCE_GAMEPAD, 0, &report, sizeof(report))) pending_tt_buttons=0;
    }
  }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
  if (instance != BEEF_HID_INSTANCE_CONFIG ||
      report_type != HID_REPORT_TYPE_FEATURE) {
    return 0;
  }
  return beef_protocol_get_report(report_id, buffer, reqlen);
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
  if (instance == BEEF_HID_INSTANCE_GAMEPAD &&
      report_type == HID_REPORT_TYPE_OUTPUT &&
      bufsize >= 8u) {
    (void)report_id;
    host_led_buttons = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
    memcpy(host_tt_rgb, buffer + 2, sizeof(host_tt_rgb));
    memcpy(host_center_rgb, buffer + 5, sizeof(host_center_rgb));
    host_lighting_ms = to_ms_since_boot(get_absolute_time());
    host_lighting_seen = true;
    return;
  }

  if (instance == BEEF_HID_INSTANCE_CONFIG &&
      report_type == HID_REPORT_TYPE_FEATURE) {
    beef_protocol_set_report(report_id, buffer, bufsize);
  }
}

int main(void) {
  controller_hardware_init();
  beef_protocol_init();

  raw_buttons = sample_raw_buttons();
  stable_buttons = raw_buttons;
  previous_raw_buttons = raw_buttons;
  logical_buttons = remap_buttons(stable_buttons);

  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(0, &dev_init);

  uint32_t last_scan_ms = to_ms_since_boot(get_absolute_time());

  while (1) {
    tud_task();
    beef_protocol_task();

    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool host_active = host_lighting_seen &&
        (uint32_t)(now - host_lighting_ms) < 1000u;
    phoenixwan_uart_task(now, beef_config_store_data(), uart_tt_direction,
                         now - uart_tt_motion_ms, uart_tt_interval_ms,
                         lighting.level, host_active);
    if (now != last_scan_ms) {
      last_scan_ms = now;

      update_button_debounce(now);
      process_turntable(now);
      logical_buttons = controller_buttons();
      update_button_leds(now);
      beef_lighting_update(&lighting,beef_config_store_data(),now,turntable.output.position,turntable.output.direction,stable_buttons);
      update_rgb_leds(now);
      update_diagnostics();
      send_controller_report();
    }

  }
}
