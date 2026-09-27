#pragma once
#include <stdint.h>

struct TurntableSample {
  uint8_t position;
  int8_t direction;
};

// One sample per millisecond, including digital press/release transitions.
class TurntableDelay {
public:
  void reset(uint32_t now, TurntableSample value, uint8_t delay) {
    for (uint16_t i = 0; i < 256; ++i) history[i] = value;
    last_time = now;
    previous = output = value;
    delay_ms = delay;
  }

  TurntableSample update(uint32_t now, TurntableSample value, uint8_t delay) {
    if (delay != delay_ms) reset(now, output, delay);
    const uint32_t elapsed = now - last_time;
    if (elapsed >= 256) {
      for (uint16_t i = 0; i < 256; ++i) history[i] = previous;
    } else {
      for (uint16_t step = 1; step <= elapsed; ++step)
        history[static_cast<uint8_t>(last_time + step)] = previous;
    }
    history[static_cast<uint8_t>(now)] = value;
    last_time = now;
    previous = value;
    output = delay == 0 ? value : history[static_cast<uint8_t>(now - delay)];
    return output;
  }

private:
  TurntableSample history[256] = {};
  TurntableSample previous = {}, output = {};
  uint32_t last_time = 0;
  uint8_t delay_ms = 0;
};
