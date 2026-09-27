#pragma once
#include <stdint.h>
struct diagnostics_report {
  uint16_t buttons;
  uint8_t sensor_ab, raw_position, filtered_position, output_position;
  int8_t direction;
  uint32_t transitions;
  uint16_t last_transition_ms;
  uint8_t led_levels[11];
} __attribute__((packed));
extern volatile diagnostics_report diagnostics;
