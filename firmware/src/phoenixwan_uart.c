#include "phoenixwan_uart.h"

#include <stdbool.h>
#include "hardware/uart.h"

// Both Phoenixwan peripherals use TTL UART, 115200 8N1. Process a bounded
// number of received bytes per call so a chatty ring cannot starve USB/input.
enum { WAIT_START, WAIT_MARKER, WAIT_REPLY, READY };
static uint8_t tt_state, tt_challenge, tt_mode, tt_motion;
static uint8_t bar_state, bar_count;
static uint32_t tt_progress_ms, bar_progress_ms;
static uint32_t last_trigger_ms;

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
}

static void receive_turntable(uint32_t now) {
  if (tt_state != WAIT_START && (uint32_t)(now - tt_progress_ms) > 500u) {
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

static uint8_t desired_bar_count(const uint8_t *cfg, uint8_t level,
                                  bool host_active) {
  if (cfg[5]) return 0x4c;
  if (cfg[84]) return cfg[2] == 9 ? 0x4c : 0x6f;
  switch (cfg[4]) {
    case 8: return 0x6f; // Static: all 16 LEDs
    case 1: case 2: {
      unsigned pairs = ((unsigned)level + 1u) / 2u;
      if (pairs > 8u) pairs = 8u;
      return pairs ? (uint8_t)(0x67u + pairs) : 0x4c;
    }
    case 5: case 6: return 0x68; // Moving tape approximated by center pair
    case 4: return host_active ? 0x6f : 0x4c;
    default: return 0x4c;
  }
}

void phoenixwan_uart_task(uint32_t now, const uint8_t *cfg,
                          int8_t direction, uint32_t motion_age_ms,
                          uint16_t motion_interval_ms,
                          uint8_t level, bool host_active) {
  receive_turntable(now);
  receive_bar(now);

  if (tt_state == READY) {
    uint8_t mode = desired_tt_mode(cfg);
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
  }

  if (bar_state == READY) {
    uint8_t count = desired_bar_count(cfg, level, host_active);
    if (count != bar_count && send_byte(uart1, count)) bar_count = count;
  }
}
