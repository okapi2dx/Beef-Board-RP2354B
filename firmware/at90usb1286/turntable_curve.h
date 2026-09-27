#pragma once
#include <stdint.h>
#include "config.h"

class TurntableCurveFilter {
public:
  void reset(uint8_t input) { previous = output = input; fraction = 0; initialized = true; }
  uint8_t update(uint8_t input, uint32_t elapsed, TurntableCurve curve) {
    if (!initialized) reset(input);
    int16_t delta = static_cast<int16_t>(input) - previous;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;
    previous = input;
    int16_t scaled = delta;
    if (curve == TurntableCurve::Precision && elapsed >= 8) {
      fraction += delta; scaled = fraction / 2; fraction -= scaled * 2;
    } else if (curve == TurntableCurve::Dynamic && elapsed <= 3) {
      scaled = delta * 2; fraction = 0;
    } else fraction = 0;
    output = static_cast<uint8_t>(output + scaled);
    return output;
  }
private:
  uint8_t previous = 0, output = 0;
  int16_t fraction = 0;
  bool initialized = false;
};
