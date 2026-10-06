#pragma once
#include <stdint.h>
// Twelfths per encoder count: 1..6 = 1/6..1; 7..10 = 1.25..2.
static inline uint8_t beef_sensitivity_units(uint8_t value) {
  if (value < 1) value = 1;
  if (value > 10) value = 10;
  return value <= 6 ? value * 2u : 12u + (value - 6u) * 3u;
}
static inline uint8_t beef_migrate_sensitivity(uint8_t old) {
  if (old < 1) return 1;
  if (old > 10) old = 10;
  return (old * 6u + 5u) / 10u;
}

static inline uint16_t beef_sensitivity_milli(uint8_t value) { return (beef_sensitivity_units(value) * 1000u + 6u) / 12u; }
