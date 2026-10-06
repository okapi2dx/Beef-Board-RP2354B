#pragma once
#include <stdint.h>

// The position has already passed the angle gate. Hold only accepted motion.
class TurntableDirection {
public:
  void reset(uint8_t position, uint32_t now) {
    previous = position; last_motion = now; direction = 0;
  }
  int8_t update(uint8_t position, uint32_t now, uint8_t sustain) {
    int16_t delta = static_cast<int16_t>(position) - previous;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;
    previous = position;
    return update_steps(delta, now, sustain);
  }
  int8_t update_steps(int32_t steps, uint32_t now, uint8_t sustain) {
    if (steps) { direction = steps > 0 ? 1 : -1; last_motion = now; }
    else if (static_cast<uint32_t>(now - last_motion) >= sustain) direction = 0;
    return direction;
  }

private:
  uint8_t previous = 0;
  uint32_t last_motion = 0;
  int8_t direction = 0;
};
