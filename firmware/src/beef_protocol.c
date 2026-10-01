#include "beef_protocol.h"

#include <string.h>
#include "beef_config_store.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "tusb.h"

#ifndef BEEF_FW_VERSION_MAJOR
#define BEEF_FW_VERSION_MAJOR 1
#endif
#ifndef BEEF_FW_VERSION_MINOR
#define BEEF_FW_VERSION_MINOR 0
#endif
#ifndef BEEF_FW_VERSION_PATCH
#define BEEF_FW_VERSION_PATCH 0
#endif

#define CONFIG_SAVE_DELAY_MS 750u
#define BEEF_CAP_DIAGNOSTICS (1u << 0)
#define BEEF_CAP_TURNTABLE   (1u << 1)
#define BEEF_CAP_WS2812      (1u << 2)
#define BEEF_CAP_CENTER_BAR  (1u << 3)
#define BEEF_CAP_UART        (1u << 4)
#define BEEF_CAP_BOOTLOADER  (1u << 5)

static volatile uint8_t pending_command = BEEF_COMMAND_NONE;
static uint32_t command_received_ms;
static uint32_t config_changed_ms;
static uint32_t diagnostics_ms;
static uint16_t diagnostics_sent_buttons;
static uint8_t diagnostics_data[BEEF_DIAGNOSTICS_SIZE];
static uint8_t chatter_counts_data[BEEF_CHATTER_COUNTS_SIZE];
static bool chatter_counts_pending;

static uint16_t copy_report(uint8_t *dst, uint16_t requested,
                            uint8_t const *src, uint16_t size) {
  uint16_t n = requested < size ? requested : size;
  memcpy(dst, src, n);
  return n;
}

static void force_usb_bus_disconnect(void) {
  tud_disconnect();
  sleep_ms(25);
}
void beef_protocol_early_boot(void) {}
static __attribute__((noreturn)) void request_hid_bootloader_reset(void) {
  force_usb_bus_disconnect();
  reset_usb_boot(0, 0);
  while (true) tight_loop_contents();
}

void beef_protocol_init(void) {
  beef_config_store_init();
  pending_command = BEEF_COMMAND_NONE;
  command_received_ms = 0;
  config_changed_ms = 0;
  diagnostics_ms = 0;
  diagnostics_sent_buttons = 0;
  memset(diagnostics_data, 0, sizeof(diagnostics_data));
  memset(chatter_counts_data, 0, sizeof(chatter_counts_data));
  chatter_counts_pending = false;
}

void beef_protocol_set_diagnostics(uint8_t const data[BEEF_DIAGNOSTICS_SIZE]) {
  memcpy(diagnostics_data, data, BEEF_DIAGNOSTICS_SIZE);
}

void beef_protocol_set_chatter_counts(uint8_t const data[BEEF_CHATTER_COUNTS_SIZE]) {
  memcpy(chatter_counts_data, data, BEEF_CHATTER_COUNTS_SIZE);
}

uint16_t beef_protocol_get_report(uint8_t report_id, uint8_t *buffer, uint16_t reqlen) {
  switch (report_id) {
    case BEEF_REPORT_CONFIG:
      return copy_report(buffer, reqlen, beef_config_store_data(), BEEF_CONFIG_SIZE);

    case BEEF_REPORT_COMMAND: {
      uint8_t value = BEEF_COMMAND_NONE;
      return copy_report(buffer, reqlen, &value, 1);
    }

    case BEEF_REPORT_FIRMWARE_VERSION: {
      uint8_t const version[7] = {
        0, 0, 0, 0,
        BEEF_FW_VERSION_MAJOR,
        BEEF_FW_VERSION_MINOR,
        BEEF_FW_VERSION_PATCH
      };
      return copy_report(buffer, reqlen, version, sizeof(version));
    }

    case BEEF_REPORT_DIAGNOSTICS:
      return copy_report(buffer, reqlen, diagnostics_data, sizeof(diagnostics_data));

    case BEEF_REPORT_BOARD_INFO: {
      uint16_t const caps = BEEF_CAP_DIAGNOSTICS |
                            BEEF_CAP_TURNTABLE |
                            BEEF_CAP_WS2812 |
                            BEEF_CAP_CENTER_BAR |
                            BEEF_CAP_UART |
                            BEEF_CAP_BOOTLOADER;
      uint8_t const info[11] = {
        0x42, 0x45, 0x45, 0x46,
        1,
        3,
        2, 0,
        (uint8_t)(caps & 0xffu), (uint8_t)(caps >> 8),
        4
      };
      return copy_report(buffer, reqlen, info, sizeof(info));
    }

    default:
      return 0;
  }
}

void beef_protocol_set_report(uint8_t report_id, uint8_t const *buffer, uint16_t size) {
  if (report_id == BEEF_REPORT_CONFIG) {
    if (size == BEEF_CONFIG_SIZE && beef_config_store_stage(buffer)) {
      config_changed_ms = to_ms_since_boot(get_absolute_time());
    }
    return;
  }

  if (report_id == BEEF_REPORT_COMMAND && size == 1) {
    if (buffer[0] >= BEEF_COMMAND_BOOTLOADER && buffer[0] <= BEEF_COMMAND_RESTART) {
      pending_command = buffer[0];
      command_received_ms = to_ms_since_boot(get_absolute_time());
    }
  }
}

void beef_protocol_task(void) {
  uint32_t const now = to_ms_since_boot(get_absolute_time());

  if (beef_config_store_is_dirty() &&
      (uint32_t)(now - config_changed_ms) >= CONFIG_SAVE_DELAY_MS) {
    if (!beef_config_store_flush()) config_changed_ms = now;
  }

  if (tud_mounted() && tud_hid_n_ready(BEEF_HID_INSTANCE_CONFIG)) {
    if (chatter_counts_pending) {
      if (tud_hid_n_report(BEEF_HID_INSTANCE_CONFIG,
                           BEEF_REPORT_CHATTER_COUNTS,
                           chatter_counts_data, sizeof(chatter_counts_data)))
        chatter_counts_pending = false;
    } else if (((uint16_t)(diagnostics_data[0] | ((uint16_t)diagnostics_data[1] << 8)) != diagnostics_sent_buttons ||
                (uint32_t)(now - diagnostics_ms) >= 20u) &&
               tud_hid_n_report(BEEF_HID_INSTANCE_CONFIG,
                                BEEF_REPORT_DIAGNOSTICS_STREAM,
                                diagnostics_data, sizeof(diagnostics_data))) {
      diagnostics_sent_buttons = (uint16_t)(diagnostics_data[0] | ((uint16_t)diagnostics_data[1] << 8));
      if ((uint32_t)(now - diagnostics_ms) >= 20u) {
        diagnostics_ms = now;
        chatter_counts_pending = true;
      }
    }
  }

  uint8_t const command = pending_command;
  if (command == BEEF_COMMAND_NONE ||
      (uint32_t)(now - command_received_ms) < 100u) {
    return;
  }

  bool saved = command == BEEF_COMMAND_RESET_CONFIG
      ? beef_config_store_reset() : beef_config_store_flush();
  if (!saved) { command_received_ms = now; return; }
  pending_command = BEEF_COMMAND_NONE;

  switch (command) {
    case BEEF_COMMAND_BOOTLOADER:
      request_hid_bootloader_reset();
      break;

    case BEEF_COMMAND_RESET_CONFIG:
      force_usb_bus_disconnect();
      watchdog_reboot(0, 0, 0);
      break;

    case BEEF_COMMAND_RESTART:
      force_usb_bus_disconnect();
      watchdog_reboot(0, 0, 0);
      break;

    default:
      break;
  }
}
