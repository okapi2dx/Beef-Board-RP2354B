#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "tusb.h"

#include "beef_config_store.h"
#include "beef_protocol.h"

#define CFG_REVERSE_TT               1u
#define CFG_TT_EFFECT                2u
#define CFG_BAR_EFFECT               4u
#define CFG_DISABLE_LEDS              5u
#define CFG_TT_STATIC_HSV             6u
#define CFG_TT_SPIN_HSV               9u
#define CFG_TT_SHIFT_HSV             12u
#define CFG_TT_RAINBOW_STATIC_HSV    15u
#define CFG_TT_RAINBOW_REACT_HSV     18u
#define CFG_TT_RAINBOW_SPIN_HSV      21u
#define CFG_TT_REACT_HSV             24u
#define CFG_TT_BREATHING_HSV         27u
#define CFG_TT_RATIO                 30u
#define CFG_IIDX_INPUT_MODE          32u
#define CFG_TT_SUSTAIN_MS            34u
#define CFG_IIDX_BUTTONS_DEBOUNCE    75u
#define CFG_LED_REFRESH              78u
#define CFG_TT_LEDS                  80u
#define CFG_BAR_STATIC_HSV           81u
#define CFG_DIGITAL_TT               85u
#define CFG_TT_DELAY_MS              86u
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

#define TT_EFFECT_HID     8u
#define TT_EFFECT_DISABLE 9u
#define BAR_EFFECT_HID     4u
#define BAR_EFFECT_DISABLE 7u

#define BUTTON_COUNT 11u
#define WS2812_MAX_LEDS 64u

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb8_t;

typedef struct {
  rgb8_t color;
  uint8_t count;
  bool valid;
} ws2812_cache_t;
#define TT_EVENT_QUEUE_SIZE 64u

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

typedef struct {
  uint32_t due_ms;
  int8_t direction;
} tt_event_t;

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

static uint8_t encoder_prev;
static int32_t tt_sensitivity_accumulator;
static uint8_t tt_x = 127u;
static int8_t tt_direction;
static uint32_t tt_transitions;
static uint32_t tt_last_transition_ms;
static uint32_t tt_last_applied_ms;
static uint32_t tt_repeat_due_ms;
static uint32_t tt_sustain_until_ms;

static tt_event_t tt_queue[TT_EVENT_QUEUE_SIZE];
static uint8_t tt_queue_head;
static uint8_t tt_queue_tail;

static diagnostics_t diagnostics;
static uint8_t previous_input_mode = 0xffu;

static uint16_t host_led_buttons;
static uint8_t host_tt_rgb[3];
static uint8_t host_center_rgb[3];
static uint32_t host_lighting_ms;
static bool host_lighting_seen;
static ws2812_cache_t tt_rgb_cache;
static ws2812_cache_t center_rgb_cache;

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

static rgb8_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
  if (s == 0u) return (rgb8_t){v, v, v};

  uint8_t region = h / 43u;
  uint8_t remainder = (uint8_t)((h - region * 43u) * 6u);

  uint8_t p = (uint8_t)(((uint16_t)v * (255u - s)) >> 8);
  uint8_t q = (uint8_t)(((uint16_t)v *
                         (255u - (((uint16_t)s * remainder) >> 8))) >> 8);
  uint8_t t = (uint8_t)(((uint16_t)v *
                         (255u - (((uint16_t)s * (255u - remainder)) >> 8))) >> 8);

  switch (region) {
    case 0: return (rgb8_t){v, t, p};
    case 1: return (rgb8_t){q, v, p};
    case 2: return (rgb8_t){p, v, t};
    case 3: return (rgb8_t){p, q, v};
    case 4: return (rgb8_t){t, p, v};
    default: return (rgb8_t){v, p, q};
  }
}

static rgb8_t config_hsv(uint8_t const *cfg, uint32_t offset) {
  return hsv_to_rgb(cfg[offset], cfg[offset + 1u], cfg[offset + 2u]);
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
static void ws2812_send_solid(uint sm, rgb8_t color, uint8_t count) {
  if (count > WS2812_MAX_LEDS) count = WS2812_MAX_LEDS;
  uint32_t grb = ((uint32_t)color.g << 16) | ((uint32_t)color.r << 8) | color.b;
  for (uint32_t i = 0; i < count; ++i) pio_sm_put_blocking(rgb_pio, sm, grb << 8);
  while (!pio_sm_is_tx_fifo_empty(rgb_pio, sm)) tight_loop_contents();
  sleep_us(80);
}

static bool rgb_equal(rgb8_t a, rgb8_t b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

static void ws2812_update_if_changed(uint sm, rgb8_t color, uint8_t count,
                                     ws2812_cache_t *cache) {
  if (count > WS2812_MAX_LEDS) count = WS2812_MAX_LEDS;
  if (cache->valid && cache->count == count && rgb_equal(cache->color, color)) return;

  ws2812_send_solid(sm, color, count);
  cache->color = color;
  cache->count = count;
  cache->valid = true;
}

static rgb8_t turntable_color(uint8_t const *cfg, uint32_t now) {
  if (cfg[CFG_DISABLE_LEDS]) return (rgb8_t){0, 0, 0};

  uint8_t const effect = cfg[CFG_TT_EFFECT];
  if (effect == TT_EFFECT_DISABLE) return (rgb8_t){0, 0, 0};
  if (effect == TT_EFFECT_HID) {
    bool const host_active = host_lighting_seen &&
        (uint32_t)(now - host_lighting_ms) < 1000u;
    return host_active
        ? (rgb8_t){host_tt_rgb[0], host_tt_rgb[1], host_tt_rgb[2]}
        : (rgb8_t){0, 0, 0};
  }

  uint32_t offset = CFG_TT_STATIC_HSV;
  switch (effect) {
    case 1: offset = CFG_TT_SPIN_HSV; break;
    case 2: offset = CFG_TT_SHIFT_HSV; break;
    case 3: offset = CFG_TT_RAINBOW_STATIC_HSV; break;
    case 4: offset = CFG_TT_RAINBOW_REACT_HSV; break;
    case 5: offset = CFG_TT_RAINBOW_SPIN_HSV; break;
    case 6: offset = CFG_TT_REACT_HSV; break;
    case 7: offset = CFG_TT_BREATHING_HSV; break;
    default: break;
  }
  return config_hsv(cfg, offset);
}

static rgb8_t center_bar_color(uint8_t const *cfg, uint32_t now) {
  if (cfg[CFG_DISABLE_LEDS]) return (rgb8_t){0, 0, 0};

  uint8_t const effect = cfg[CFG_BAR_EFFECT];
  if (effect == BAR_EFFECT_DISABLE) return (rgb8_t){0, 0, 0};
  if (effect == BAR_EFFECT_HID) {
    bool const host_active = host_lighting_seen &&
        (uint32_t)(now - host_lighting_ms) < 1000u;
    return host_active
        ? (rgb8_t){host_center_rgb[0], host_center_rgb[1], host_center_rgb[2]}
        : (rgb8_t){0, 0, 0};
  }
  return config_hsv(cfg, CFG_BAR_STATIC_HSV);
}

static void update_rgb_leds(uint32_t now) {
  uint8_t const *cfg = beef_config_store_data();
  ws2812_update_if_changed(rgb_sm_tt,
                           turntable_color(cfg, now), cfg[CFG_TT_LEDS],
                           &tt_rgb_cache);
  ws2812_update_if_changed(rgb_sm_bar,
                           center_bar_color(cfg, now), cfg[CFG_BAR_LEDS],
                           &center_rgb_cache);
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
  uart_init(uart0, 115200);
  gpio_set_function(18, GPIO_FUNC_UART);
  gpio_set_function(19, GPIO_FUNC_UART);
  uart_init(uart1, 115200);
  gpio_set_function(22, GPIO_FUNC_UART);
  gpio_set_function(23, GPIO_FUNC_UART);
  encoder_prev = (gpio_get(21) ? 1u : 0u) | (gpio_get(20) ? 2u : 0u);
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

static int8_t read_encoder_transition(void) {
  static int8_t const transitions[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
  };

  uint8_t now =
      (gpio_get(21) ? 1u : 0u) |
      (gpio_get(20) ? 2u : 0u);

  int8_t direction = transitions[(encoder_prev << 2) | now];
  encoder_prev = now;

  if (beef_config_store_data()[CFG_REVERSE_TT]) direction = (int8_t)-direction;
  return direction;
}

static bool tt_queue_empty(void) {
  return tt_queue_head == tt_queue_tail;
}

static void tt_queue_push(int8_t direction, uint32_t due_ms) {
  uint8_t next = (uint8_t)((tt_queue_head + 1u) % TT_EVENT_QUEUE_SIZE);
  if (next == tt_queue_tail) {
    /* Keep the newest turntable data if the host stalls for too long. */
    tt_queue_tail = (uint8_t)((tt_queue_tail + 1u) % TT_EVENT_QUEUE_SIZE);
  }

  tt_queue[tt_queue_head].direction = direction;
  tt_queue[tt_queue_head].due_ms = due_ms;
  tt_queue_head = next;
}

static bool tt_time_reached(uint32_t now, uint32_t due) {
  return (int32_t)(now - due) >= 0;
}

static void apply_analog_tt_step(int8_t direction) {
  uint8_t sensitivity = beef_config_store_data()[CFG_TT_RATIO];
  if (sensitivity < 1u) sensitivity = 1u;
  if (sensitivity > 10u) sensitivity = 10u;

  tt_sensitivity_accumulator += (int32_t)direction * sensitivity;

  while (tt_sensitivity_accumulator >= 10) {
    ++tt_x;
    tt_sensitivity_accumulator -= 10;
  }
  while (tt_sensitivity_accumulator <= -10) {
    --tt_x;
    tt_sensitivity_accumulator += 10;
  }
}

static void apply_tt_event(int8_t direction, uint32_t now, bool synthetic) {
  uint8_t const *cfg = beef_config_store_data();

  tt_direction = direction;
  if (!synthetic) {
    ++tt_transitions;
    tt_last_transition_ms = now;
  }

  if (!cfg[CFG_DIGITAL_TT]) {
    apply_analog_tt_step(direction);
  }

  uint32_t interval = tt_last_applied_ms == 0u ? 1u : now - tt_last_applied_ms;
  if (interval < 1u) interval = 1u;
  if (interval > 20u) interval = 20u;
  tt_last_applied_ms = now;
  tt_repeat_due_ms = now + interval;

  uint32_t sustain = cfg[CFG_TT_SUSTAIN_MS];
  tt_sustain_until_ms = now + sustain;
}

static bool process_delayed_tt_events(uint32_t now) {
  bool applied = false;

  while (!tt_queue_empty()) {
    tt_event_t const *event = &tt_queue[tt_queue_tail];
    if (!tt_time_reached(now, event->due_ms)) break;

    apply_tt_event(event->direction, now, false);
    tt_queue_tail = (uint8_t)((tt_queue_tail + 1u) % TT_EVENT_QUEUE_SIZE);
    applied = true;
  }

  return applied;
}

static void process_turntable(uint32_t now) {
  uint8_t const *cfg = beef_config_store_data();
  int8_t transition = read_encoder_transition();

  if (transition != 0) {
    tt_queue_push(transition, now + cfg[CFG_TT_DELAY_MS]);
  }

  bool applied = process_delayed_tt_events(now);

  if (!applied && cfg[CFG_TT_SUSTAIN_MS] != 0u && tt_direction != 0 &&
      !cfg[CFG_DIGITAL_TT] &&
      !tt_time_reached(now, tt_sustain_until_ms) &&
      tt_time_reached(now, tt_repeat_due_ms)) {
    apply_tt_event(tt_direction, now, true);
  }

  if (cfg[CFG_DIGITAL_TT]) {
    bool hold = applied ||
                (cfg[CFG_TT_SUSTAIN_MS] != 0u &&
                 !tt_time_reached(now, tt_sustain_until_ms));
    if (!hold) tt_direction = 0;
  } else if (tt_time_reached(now, tt_sustain_until_ms)) {
    tt_direction = 0;
  }
}

static uint16_t controller_buttons(void) {
  uint16_t result = remap_buttons(stable_buttons);
  uint8_t const *cfg = beef_config_store_data();

  if (cfg[CFG_DIGITAL_TT]) {
    if (tt_direction < 0) result |= (uint16_t)(1u << 11);
    if (tt_direction > 0) result |= (uint16_t)(1u << 12);
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
  diagnostics.buttons = logical_buttons;
  diagnostics.sensor_ab =
      (gpio_get(21) ? 1u : 0u) |
      (gpio_get(20) ? 2u : 0u);
  diagnostics.raw_position = tt_x;
  diagnostics.filtered_position = tt_x;
  diagnostics.output_position = tt_x;
  diagnostics.direction = tt_direction;
  diagnostics.transitions = tt_transitions;
  diagnostics.last_transition_ms = (uint16_t)tt_last_transition_ms;

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
    uint8_t keys[13] = {0};
    uint32_t used = 0;

    for (uint32_t i = 0; i < BUTTON_COUNT && used < 13u; ++i) {
      if ((logical_buttons & (1u << i)) != 0u) {
        uint8_t key = cfg[35u + i];
        if (key != 0u) keys[used++] = key;
      }
    }

    if (tud_hid_n_ready(BEEF_HID_INSTANCE_KEYBOARD)) {
      (void)tud_hid_n_report(BEEF_HID_INSTANCE_KEYBOARD, 0, keys, sizeof(keys));
    }
  } else {
    uint16_t lower = logical_buttons & 0x007fu;
    uint16_t upper = logical_buttons >> 7;
    uint16_t iidx_buttons = lower | (uint16_t)(upper << 8);

    gamepad_payload_t report = {
      .x = cfg[CFG_DIGITAL_TT] ? 127u : tt_x,
      .y = 127u,
      .buttons = iidx_buttons
    };
    if (tud_hid_n_ready(BEEF_HID_INSTANCE_GAMEPAD)) {
      (void)tud_hid_n_report(BEEF_HID_INSTANCE_GAMEPAD, 0, &report, sizeof(report));
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
    if (now != last_scan_ms) {
      last_scan_ms = now;

      update_button_debounce(now);
      process_turntable(now);
      logical_buttons = controller_buttons();
      update_button_leds(now);
      update_rgb_leds(now);
      update_diagnostics();
      send_controller_report();
    }

  }
}
