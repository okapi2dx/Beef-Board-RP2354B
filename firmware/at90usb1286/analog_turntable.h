#pragma once
#include <stdint.h>

// Follow physical X-axis movement one-for-one. After encoder input stops,
// sustain advances X at a fixed 100 counts/second in the last direction.
// No physical speed or cadence is measured.
class AnalogTurntable {
public:
  void reset(uint8_t position, uint32_t now) {
    anchor = position;
    output = position;
    last_motion_time = now;
    last_update_time = now;
    hold_remainder = 0;
    direction = 0;
  }

  uint8_t update(uint8_t raw, uint32_t now, uint8_t deadzone, uint8_t sustain,
                 uint32_t encoder_activity) {
    (void)encoder_activity;

    int16_t delta = static_cast<int16_t>(raw) - anchor;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;

    if (delta >= deadzone || delta <= -static_cast<int16_t>(deadzone)) {
      output += delta;
      anchor = raw;
      direction = delta > 0 ? 1 : -1;
      last_motion_time = now;
      last_update_time = now;
      hold_remainder = 0;
      return static_cast<uint8_t>(output);
    }

    if (sustain == 0 || direction == 0) {
      last_update_time = now;
      hold_remainder = 0;
      return static_cast<uint8_t>(output);
    }

    const uint32_t hold_now = clamp_hold(now - last_motion_time, sustain);
    const uint32_t hold_prev = clamp_hold(last_update_time - last_motion_time, sustain);
    if (hold_now > hold_prev) {
      const uint32_t numerator = hold_remainder + (hold_now - hold_prev) * 100UL;
      const uint16_t counts = static_cast<uint16_t>(numerator / 1000UL);
      hold_remainder = static_cast<uint16_t>(numerator % 1000UL);
      output += static_cast<int32_t>(direction) * counts;
    }

    last_update_time = now;
    return static_cast<uint8_t>(output);
  }

private:
  static uint32_t clamp_hold(uint32_t elapsed, uint8_t sustain) {
    return elapsed < sustain ? elapsed : sustain;
  }

  uint8_t anchor = 0;
  int32_t output = 0;
  uint32_t last_motion_time = 0;
  uint32_t last_update_time = 0;
  uint16_t hold_remainder = 0;
  int8_t direction = 0;
};
