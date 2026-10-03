#include "phoenixwan_uart.h"

#include <stdbool.h>
#include <stddef.h>
#include "hardware/uart.h"

// Both Phoenixwan peripherals use TTL UART, 115200 8N1. Process a bounded
// number of received bytes per call so a chatty ring cannot starve USB/input.
enum { WAIT_START, WAIT_MARKER, WAIT_REPLY, READY };
static uint8_t tt_state, tt_challenge, tt_mode, tt_motion;
static uint8_t bar_state, bar_count;
static uint32_t tt_progress_ms, bar_progress_ms;
static uint32_t last_trigger_ms;
static uint8_t tt_color[3], tt_color_cursor;
static int16_t tt_color_pending[3];
static bool tt_color_seen;
static bool previous_enabled;
static uint8_t tt_adjust_queue[8], tt_adjust_head, tt_adjust_count;
static uint8_t bar_adjust_queue[8], bar_adjust_head, bar_adjust_count;

enum { COLOR_CONFIG_STEP = 16 };

static bool send_byte(uart_inst_t *port, uint8_t byte) {
  if (!uart_is_writable(port)) return false;
  uart_putc_raw(port, byte);
  return true;
}

static uint8_t challenge_reply(uint8_t index) {
  // The six challenges are all zero. Each reply applies the nibble operation
  // documented for its position in the Phoenixwan initialization exchange.
  static const uint8_t reply[6] = {0x5f, 0x78, 0x55, 0x74, 0xaf, 0xf8};
  return reply[index];
}

void phoenixwan_uart_init(void) {
  uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
  uart_set_hw_flow(uart0, false, false);
  uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
  uart_set_hw_flow(uart1, false, false);
  tt_state = bar_state = WAIT_START;
  tt_mode = tt_motion = bar_count = 0xff;
  tt_color_seen = false;
  tt_adjust_head = tt_adjust_count = 0;
  bar_adjust_head = bar_adjust_count = 0;
}

void phoenixwan_uart_adjust_turntable_color(uint8_t channel, bool increase) {
  if (channel >= 3u || tt_adjust_count >= sizeof(tt_adjust_queue)) return;
  uint8_t tail = (uint8_t)((tt_adjust_head + tt_adjust_count) % sizeof(tt_adjust_queue));
  tt_adjust_queue[tail] = (uint8_t)((increase ? 0x81u : 0x91u) + channel);
  ++tt_adjust_count;
}

void phoenixwan_uart_adjust_centerbar_color(uint8_t channel, bool increase) {
  if (channel >= 3u || bar_adjust_count >= sizeof(bar_adjust_queue)) return;
  uint8_t tail = (uint8_t)((bar_adjust_head + bar_adjust_count) % sizeof(bar_adjust_queue));
  bar_adjust_queue[tail] = (uint8_t)((increase ? 0x81u : 0x91u) + channel);
  ++bar_adjust_count;
}

static void receive_turntable(uint32_t now) {
  if ((tt_state == WAIT_MARKER || tt_state == WAIT_REPLY) &&
      (uint32_t)(now - tt_progress_ms) > 500u) {
    tt_state = WAIT_START;
  }
  for (unsigned n = 0; n < 32 && uart_is_readable(uart0); ++n) {
    uint8_t byte = uart_getc(uart0);
    if (tt_state == READY) {
      if (byte == 0x5a) {
        tt_state = WAIT_MARKER;
        tt_progress_ms = now;
        (void)send_byte(uart0, 0x5a);
      }
    } else if (tt_state == WAIT_START) {
      if (byte == 0x5a && send_byte(uart0, 0x5a)) {
        tt_state = WAIT_MARKER;
        tt_progress_ms = now;
      }
    } else if (tt_state == WAIT_MARKER) {
      if (byte == 0x7e && send_byte(uart0, 0x00)) {
        tt_challenge = 0;
        tt_state = WAIT_REPLY;
        tt_progress_ms = now;
      }
    } else if (byte == challenge_reply(tt_challenge)) {
      tt_progress_ms = now;
      if (++tt_challenge == 6) {
        if (send_byte(uart0, 0xa5)) {
          tt_state = READY;
          tt_mode = tt_motion = 0xff;
          tt_color_seen = false;
        } else tt_state = WAIT_START;
      } else if (!send_byte(uart0, 0x00)) {
        tt_state = WAIT_START;
      }
    } else {
      tt_state = WAIT_START;
    }
  }
}

static void receive_bar(uint32_t now) {
  if (bar_state == WAIT_MARKER && (uint32_t)(now - bar_progress_ms) > 500u)
    bar_state = WAIT_START;
  for (unsigned n = 0; n < 16 && uart_is_readable(uart1); ++n) {
    uint8_t byte = uart_getc(uart1);
    if (byte == 0xf0 && send_byte(uart1, 0xa5)) {
      bar_state = WAIT_MARKER;
      bar_progress_ms = now;
    } else if (bar_state == WAIT_MARKER && byte == 0x5a) {
      bar_state = READY;
      bar_count = 0xff;
    }
  }
}

static uint8_t desired_tt_mode(const uint8_t *cfg) {
  static const uint8_t modes[8] = {0x31, 0x32, 0x33, 0x34,
                                    0x35, 0x35, 0x36, 0x37};
  uint8_t effect = cfg[2];
  // HID RGB and Off have no direct equivalent in the documented UART protocol.
  if (cfg[5] || effect >= 8) return 0;
  return modes[effect];
}

static void update_color_target(const uint8_t target[3], uint8_t previous[3],
                                int16_t pending[3], bool *seen) {
  if (!*seen) {
    for (unsigned i = 0; i < 3; ++i) {
      previous[i] = target[i];
      pending[i] = 0;
    }
    *seen = true;
    return;
  }
  for (unsigned i = 0; i < 3; ++i) {
    int delta = (int)target[i] - (int)previous[i];
    if (i == 0) {
      if (delta > 127) delta -= 256;
      if (delta < -128) delta += 256;
    }
    int total = pending[i] + delta;
    if (total > 2048) total = 2048;
    if (total < -2048) total = -2048;
    pending[i] = (int16_t)total;
    previous[i] = target[i];
  }
}

static void send_color_step(uart_inst_t *port, int16_t pending[3],
                            uint8_t enabled_mask, uint8_t *cursor) {
  for (unsigned n = 0; n < 3; ++n) {
    unsigned i = (*cursor + n) % 3u;
    if (!(enabled_mask & (1u << i))) continue;
    int16_t amount = pending[i];
    if (amount < COLOR_CONFIG_STEP && amount > -COLOR_CONFIG_STEP) continue;
    uint8_t command = (uint8_t)((amount > 0 ? 0x81u : 0x91u) + i);
    if (send_byte(port, command)) {
      pending[i] -= amount > 0 ? COLOR_CONFIG_STEP : -COLOR_CONFIG_STEP;
      *cursor = (uint8_t)((i + 1u) % 3u);
    }
    break;
  }
}

void phoenixwan_uart_task(uint32_t now, const uint8_t *cfg,
                          int8_t direction, uint32_t motion_age_ms,
  uint16_t motion_interval_ms,
                          bool enabled) {
  receive_turntable(now);
  receive_bar(now);

  if (enabled != previous_enabled) {
    tt_mode = tt_motion = bar_count = 0xff;
    tt_color_seen = false;
    tt_adjust_head = tt_adjust_count = 0;
    bar_adjust_head = bar_adjust_count = 0;
    previous_enabled = enabled;
  }
  if (!enabled) return;

  if (tt_state == READY) {
    uint8_t mode = desired_tt_mode(cfg);
    // Colors belong to separate modes. Switching modes must establish a new
    // baseline instead of applying the difference between unrelated colors.
    if (mode != tt_mode) tt_color_seen = false;
    if (mode) {
      const uint8_t *color = &cfg[6u + 3u * cfg[2]];
      update_color_target(color, tt_color, tt_color_pending, &tt_color_seen);
    }
    if (mode && mode != tt_mode) {
      if (send_byte(uart0, mode)) {
        tt_mode = mode;
        tt_motion = 0xff;
      }
    } else if (mode == 0x32 || mode == 0x35 || mode == 0x36) {
      uint8_t motion = 0;
      if (direction && motion_age_ms < 150u) {
        if (mode == 0x32) motion = direction > 0 ? 0x4e : 0x4f;
        else if (mode == 0x35) {
          unsigned speed = motion_interval_ms ? 150u / motion_interval_ms : 15u;
          if (speed < 1u) speed = 1u;
          if (speed > 15u) speed = 15u;
          motion = (uint8_t)((direction > 0 ? 0x50u : 0x60u) + speed);
        }
        else motion = 0x71;
      }
      // Keep trigger active during motion without filling the UART TX FIFO.
      if (mode == 0x36 && motion && (uint32_t)(now - last_trigger_ms) >= 50u) {
        tt_motion = 0xff;
        last_trigger_ms = now;
      }
      if (motion != tt_motion && send_byte(uart0, motion)) tt_motion = motion;
    }
    if (mode && mode == tt_mode) {
      uint8_t color_mask = mode == 0x31 ? 0x07u :
          mode == 0x32 ? 0x01u :
          mode >= 0x33 && mode <= 0x35 ? 0x06u : 0x03u;
      send_color_step(uart0, tt_color_pending, color_mask, &tt_color_cursor);
      if (tt_adjust_count) {
        uint8_t command = tt_adjust_queue[tt_adjust_head];
        uint8_t channel = (uint8_t)((command & 0x0fu) - 1u);
        if (!(color_mask & (1u << channel)) || send_byte(uart0, command)) {
          tt_adjust_head = (uint8_t)((tt_adjust_head + 1u) % sizeof(tt_adjust_queue));
          --tt_adjust_count;
        }
      }
    }
  }

  if (bar_state == READY) {
    uint8_t count = cfg[5] || !cfg[61] ? 0x4cu : (uint8_t)(0x67u + cfg[61] / 2u);
    if (count != bar_count && send_byte(uart1, count)) bar_count = count;
    if (bar_adjust_count) {
      uint8_t const command = bar_adjust_queue[bar_adjust_head];
      if (send_byte(uart1, command)) {
        bar_adjust_head = (uint8_t)((bar_adjust_head + 1u) % sizeof(bar_adjust_queue));
        --bar_adjust_count;
      }
    }
  }
}
