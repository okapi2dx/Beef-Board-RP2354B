#pragma once
#include <stdint.h>
#include "../src/turntable_sensitivity.h"

// Turntable sensitivity is expressed directly as 1..10.
// 6 = 1:1, 3 = 1:2, 10 = 2:1. Fractional progress is retained between updates.
class EncoderPosition {
public:
  void update(int8_t direction, uint8_t sensitivity) {
    if (sensitivity == 0) return;
    sensitivity = beef_sensitivity_units(sensitivity);

    const uint16_t period = 3072;
    if (direction < 0) {
      position = position < sensitivity ? period - (sensitivity - position)
                                        : position - sensitivity;
    } else if (direction > 0) {
      position += sensitivity;
      if (position >= period) position -= period;
    }
  }

  void update_milli(int8_t direction, uint16_t milli) {
    int32_t next = int32_t(milli_position) + int32_t(direction) * milli;
    if (next < 0) next += 256000;
    if (next >= 256000) next -= 256000;
    milli_position = next;
    milli_active = true;
  }
  uint8_t get() const { return static_cast<uint8_t>(milli_active ? milli_position / 1000 : position / 12); }

private:
  uint16_t position = 0;
  uint32_t milli_position = 0;
  bool milli_active = false;
};
