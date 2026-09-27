#pragma once
#include <stdint.h>

// Adapt the continuity window to the measured encoder event interval.
// Qualify only on real movement, never merely because a timer elapsed.
class TurntableGate {
public:
  void reset(uint8_t raw, uint8_t position, uint32_t now) {
    previous = raw;
    output = position;
    started = last_motion = now;
    direction = 0;
    interval = 0;
  }
  uint8_t update(uint8_t raw, uint32_t now, uint8_t threshold) {
    int16_t delta = static_cast<int16_t>(raw) - previous;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;
    previous = raw;
    if (delta == 0) return output;
    const int8_t next_direction = delta > 0 ? 1 : -1;
    const uint32_t gap = now - last_motion;
    uint32_t continuity = interval == 0 ? 250 : interval * 3;
    if (continuity < 30) continuity = 30;
    if (continuity > 1000) continuity = 1000;
    if (direction != next_direction || gap > continuity) {
      started = now;
      interval = 0;
    } else if (gap > 0) {
      interval = gap;
    }
    direction = next_direction;
    last_motion = now;
    if (threshold == 0 || now - started > threshold)
      output = static_cast<uint8_t>(output + delta);
    return output;
  }
private:
  uint8_t previous = 0, output = 0;
  int8_t direction = 0;
  uint32_t started = 0, last_motion = 0;
  uint32_t interval = 0;
};
