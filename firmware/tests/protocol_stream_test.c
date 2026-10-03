#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "beef_protocol.h"

static uint32_t clock_ms;
static bool ready = true, accept = true;
static unsigned sends;
static uint8_t last_id, last_buttons;
static uint8_t config_bytes[BEEF_CONFIG_SIZE];
static uint32_t get_absolute_time(void) { return clock_ms; }
static uint32_t to_ms_since_boot(uint32_t t) { return t; }
static void sleep_ms(unsigned ms) { (void)ms; }
static void tight_loop_contents(void) {}
static void reset_usb_boot(unsigned a, unsigned b) { (void)a; (void)b; assert(0); }
static void watchdog_reboot(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; assert(0); }
static void tud_disconnect(void) {}
static bool tud_mounted(void) { return true; }
static bool tud_hid_n_ready(unsigned instance) { (void)instance; return ready; }
static bool tud_hid_n_report(unsigned instance, uint8_t id, const void *data, unsigned size) {
  assert(instance == BEEF_HID_INSTANCE_CONFIG);
  if (!accept) return false;
  assert(size == (id == BEEF_REPORT_CHATTER_COUNTS ? 28u : 60u));
  ++sends; last_id = id; last_buttons = ((const uint8_t *)data)[0];
  return true;
}
void beef_config_store_init(void) {}
bool beef_config_store_is_dirty(void) { return false; }
bool beef_config_store_flush(void) { return true; }
bool beef_config_store_reset(void) { return true; }
bool beef_config_store_stage(const uint8_t data[BEEF_CONFIG_SIZE]) { (void)data; return true; }
const uint8_t *beef_config_store_data(void) { return config_bytes; }

static bool legacy_enabled = true;
static unsigned tt_adjustments, bar_adjustments;
static uint8_t last_channel;
static bool last_increase;
bool beef_config_store_legacy_phoenixwan_enabled(void) { return legacy_enabled; }
bool beef_config_store_set_legacy_phoenixwan_enabled(bool enabled) { legacy_enabled = enabled; return true; }
void phoenixwan_uart_adjust_turntable_color(uint8_t channel, bool increase) {
  ++tt_adjustments; last_channel=channel; last_increase=increase;
}
void phoenixwan_uart_adjust_centerbar_color(uint8_t channel, bool increase) {
  ++bar_adjustments; last_channel=channel; last_increase=increase;
}

// The runner supplies production source, removing only hardware includes.
#include "protocol_under_test.c"

int main(void) {
  uint8_t report[BEEF_DIAGNOSTICS_SIZE] = {0};
  beef_protocol_init();
  clock_ms = 1; report[0] = 1; beef_protocol_set_diagnostics(report); beef_protocol_task();
  assert(sends == 1 && last_id == BEEF_REPORT_DIAGNOSTICS_STREAM && last_buttons == 1);
  clock_ms = 2; report[0] = 0; beef_protocol_set_diagnostics(report); beef_protocol_task();
  assert(sends == 2 && last_buttons == 0); // A short output pulse is reported before 20 ms.
  clock_ms = 3; ready = false; report[0] = 1; beef_protocol_set_diagnostics(report); beef_protocol_task();
  assert(sends == 2);
  ready = true; accept = false; beef_protocol_task(); assert(sends == 2);
  accept = true; beef_protocol_task(); assert(sends == 3 && last_buttons == 1);
  beef_protocol_task(); assert(sends == 3); // Unchanged output does not flood USB.
  clock_ms = 20; beef_protocol_task(); assert(sends == 4 && last_id == BEEF_REPORT_DIAGNOSTICS_STREAM);
  clock_ms = 21; beef_protocol_task(); assert(sends == 5 && last_id == BEEF_REPORT_CHATTER_COUNTS);
  clock_ms = 22; beef_protocol_task(); assert(sends == 5);
  for (uint8_t command=6; command<=17; ++command) {
    beef_protocol_set_report(BEEF_REPORT_COMMAND, &command, 1);
    assert(last_channel == (uint8_t)((command - (command < 12 ? 6 : 12))/2));
    assert(last_increase == ((command & 1u) != 0));
  }
  assert(tt_adjustments==6 && bar_adjustments==6);
  uint8_t command=5;beef_protocol_set_report(BEEF_REPORT_COMMAND,&command,1);
  command=7;beef_protocol_set_report(BEEF_REPORT_COMMAND,&command,1);
  command=13;beef_protocol_set_report(BEEF_REPORT_COMMAND,&command,1);
  assert(tt_adjustments==6 && bar_adjustments==6);
  command=4;beef_protocol_set_report(BEEF_REPORT_COMMAND,&command,1);
  assert(legacy_enabled);
  puts("PASS protocol stream: short output pulses, USB backpressure, failed-send retry and periodic counts");
}
