#pragma once
#include <stdint.h>
#include "../src/turntable_encoder_gate.h"
#include "../src/turntable_sensitivity.h"

class TurntableGate {
public:
  void reset(uint8_t raw, uint8_t position, uint32_t now) {
    previous = raw; output = position;
    state = {}; state.last_motion = now; fraction = 0; accepted_steps_value = 0;
  }
  uint8_t update(uint8_t raw, uint32_t now, uint8_t counts) {
    int16_t delta = static_cast<int16_t>(raw) - previous;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;
    previous = raw;
    return update_steps(delta, now, counts, 6);
  }
  uint8_t update_steps(int32_t steps, uint32_t now, uint8_t counts, uint8_t sensitivity) {
    accepted_steps_value = beef_encoder_gate_update(&state, steps, now, counts);
    sensitivity = beef_sensitivity_units(sensitivity);
    fraction += accepted_steps_value * sensitivity;
    output = static_cast<uint8_t>(output + fraction / 12);
    fraction %= 12;
    return output;
  }
  uint8_t update_milli(int32_t steps, uint32_t now, uint8_t counts, uint16_t milli) {
    accepted_steps_value = beef_encoder_gate_update(&state, steps, now, counts);
    if (!milli) accepted_steps_value = 0;
    fraction += accepted_steps_value * milli;
    output = static_cast<uint8_t>(output + fraction / 1000);
    fraction %= 1000;
    return output;
  }
  int32_t accepted_steps() const { return accepted_steps_value; }
  bool reversing() const { return beef_encoder_gate_reversing(&state); }
private:
  uint8_t previous = 0, output = 0;
  beef_encoder_gate_t state = {};
  int32_t fraction = 0, accepted_steps_value = 0;
};
