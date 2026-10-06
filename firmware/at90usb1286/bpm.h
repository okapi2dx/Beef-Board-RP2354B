#pragma once

#include "timer.h"

// Doesn't actually track BPM, but it's the closest concept
class Bpm {
public:
  explicit Bpm(uint8_t max_level);

  uint8_t update(uint16_t button_state);
  void set_max_level(uint8_t value) { max_level = value; if (current_level > value) current_level = value; }

private:
  uint8_t max_level;
  timer decay{};
  timer button_guard{};
  uint8_t current_level{};
  uint16_t last_button_state{};
  uint32_t sample_time{};
};
