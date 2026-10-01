#pragma once
#include <stdint.h>
struct diagnostics_report {
  uint16_t buttons;
  uint8_t sensor_ab, raw_position, filtered_position, output_position;
  int8_t direction;
  uint32_t transitions;
  uint16_t last_transition_ms;
  uint8_t led_levels[11];
  uint16_t chatter_edges[7];
  uint16_t raw_buttons;
  uint16_t edge_sequence;
  uint8_t edge_events[18];
} __attribute__((packed));
static_assert(sizeof(diagnostics_report) == 60, "diagnostics report must fit the HID endpoint");
extern volatile diagnostics_report diagnostics;

struct chatter_counts_report {
  uint16_t edges[7];
  uint16_t detected[7];
} __attribute__((packed));
static_assert(sizeof(chatter_counts_report) == 28, "chatter counts must fit the HID endpoint");
extern volatile chatter_counts_report chatter_counts;
