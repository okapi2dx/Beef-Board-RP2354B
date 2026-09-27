#pragma once
#include <stdint.h>

// Q8 velocity and hue retain fractional steps without floating point on AVR.
class SmoothRainbow {
  bool started = false;
  uint32_t last = 0, sampled = 0, motion = 0;
  uint8_t previous = 0;
  int32_t target = 0, velocity = 0;
  uint16_t phase = 0;
public:
  uint16_t update(uint32_t now, uint8_t position, bool reverse, uint8_t speed) {
    if (!started) { started = true; last = sampled = motion = now; previous = position; return phase; }
    uint32_t elapsed = now - last;
    last = now;
    if (elapsed > 50) elapsed = 50;
    const uint32_t interval = now - sampled;
    if (interval >= 50) {
      int16_t delta = static_cast<int16_t>(position) - previous;
      if (delta > 127) delta -= 256;
      if (delta < -128) delta += 256;
      previous = position; sampled = now;
      if (delta) {
        int32_t next = static_cast<int32_t>(delta) * 1536 / static_cast<int32_t>(interval);
        if (next > 256) next = 256;
        if (next < -256) next = -256;
        if (!reverse) next = -next;
        if ((next < 0 && velocity > 0) || (next > 0 && velocity < 0)) velocity = 0;
        target = next; motion = now;
      }
    }
    const uint32_t age = now - motion;
    if (age >= 150) { target = velocity = 0; }
    const int32_t desired = age <= 60 ? target : age < 150 ? target * static_cast<int32_t>(150-age) / 90 : 0;
    // Stable first-order easing with about 40ms response, as in the app.
    velocity += (desired - velocity) * static_cast<int32_t>(elapsed) / static_cast<int32_t>(40 + elapsed);
    phase += velocity * static_cast<int32_t>(elapsed) * speed / 3;
    return phase;
  }
};
