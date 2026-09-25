#pragma once

#include <stdint.h>

#define BEEF_CONFIG_SIZE 139u
#define BEEF_DIAGNOSTICS_SIZE 24u

#define BEEF_HID_INSTANCE_GAMEPAD  0u
#define BEEF_HID_INSTANCE_KEYBOARD 1u
#define BEEF_HID_INSTANCE_CONFIG   2u

enum beef_report_id {
  BEEF_REPORT_CONFIG = 1,
  BEEF_REPORT_COMMAND = 2,
  BEEF_REPORT_FIRMWARE_VERSION = 3,
  BEEF_REPORT_DIAGNOSTICS = 4,
  BEEF_REPORT_DIAGNOSTICS_STREAM = 5,
  BEEF_REPORT_BOARD_INFO = 6
};

enum beef_command {
  BEEF_COMMAND_NONE = 0,
  BEEF_COMMAND_BOOTLOADER = 1,
  BEEF_COMMAND_RESET_CONFIG = 2,
  BEEF_COMMAND_RESTART = 3
};

void beef_protocol_early_boot(void);
void beef_protocol_init(void);
void beef_protocol_task(void);
uint16_t beef_protocol_get_report(uint8_t report_id, uint8_t *buffer, uint16_t reqlen);
void beef_protocol_set_report(uint8_t report_id, uint8_t const *buffer, uint16_t size);
void beef_protocol_set_diagnostics(uint8_t const data[BEEF_DIAGNOSTICS_SIZE]);
