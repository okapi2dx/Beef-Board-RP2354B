#pragma once
#include <stdint.h>

// Turntable sensitivity is expressed directly as 1..10.
// 10 = 1:1 (every physical count advances the HID X axis),
// 5 = 1:2, 1 = 1:10. Fractional progress is retained between updates.
class EncoderPosition {
public:
  void update(int8_t direction, uint8_t sensitivity) {
    if (sensitivity == 0) return;
    if (sensitivity > 10) sensitivity = 10;

    const uint16_t period = 2560;
    if (direction < 0) {
      position = position < sensitivity ? period - (sensitivity - position)
                                        : position - sensitivity;
    } else if (direction > 0) {
      position += sensitivity;
      if (position >= period) position -= period;
    }
  }

  uint8_t get() const { return static_cast<uint8_t>(position / 10); }

private:
  uint16_t position = 0;
};
