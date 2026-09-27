#include "../turntable_gate.h"
#include <assert.h>
#include <stdio.h>
int main() {
  for (int threshold = 0; threshold <= 255; ++threshold) {
    for (int sign = -1; sign <= 1; sign += 2) {
      TurntableGate gate;
      gate.reset(0, 0, 0);
      for (int time = 1; time <= threshold + 3; ++time) {
        const auto value = gate.update(static_cast<uint8_t>(time * sign), time, threshold);
        const int count = threshold == 0 ? time : (time > threshold + 1 ? time - threshold - 1 : 0);
        assert(value == static_cast<uint8_t>(count * sign));
      }
    }
  }
  TurntableGate gate;
  gate.reset(0, 0, 0);
  for (int t = 1; t <= 51; ++t) assert(gate.update(t, t, 50) == 0);
  assert(gate.update(51, 500, 50) == 0); // stopped: cannot qualify on elapsed time alone
  assert(gate.update(52, 501, 50) == 0); // fresh gesture
  gate.reset(0, 0, 0);
  for (int t = 1; t <= 51; ++t) assert(gate.update(t, t, 50) == 0);
  assert(gate.update(52, 52, 50) == 1); // 51 ms of real motion
  assert(gate.update(51, 53, 50) == 1); // reversal requires another qualification
  gate.reset(0, 0, 0xfffffff0UL);
  for (int t = 1; t <= 51; ++t) assert(gate.update(t, 0xfffffff0UL + t, 50) == 0);
  assert(gate.update(52, 0xfffffff0UL + 52, 50) == 1);
  puts("PASS: all thresholds 0..255, both directions, exact boundary, stop, restart, reversal and timer wrap");
  for (int spacing = 1; spacing <= 250; ++spacing) {
    for (int threshold = 1; threshold <= 255; ++threshold) {
      for (int sign = -1; sign <= 1; sign += 2) {
        gate.reset(0, 0, 0);
        uint8_t expected = 0;
        for (int edge = 1; edge <= 8; ++edge) {
          if ((edge - 1) * spacing > threshold) expected = static_cast<uint8_t>(expected + sign);
          assert(gate.update(static_cast<uint8_t>(edge * sign), edge * spacing, threshold) == expected);
        }
        // An isolated final edge must never qualify by waiting alone.
        assert(gate.update(static_cast<uint8_t>(8 * sign), 8 * spacing + 2000, threshold) == expected);
        assert(gate.update(static_cast<uint8_t>(9 * sign), 8 * spacing + 2001, threshold) == expected);
      }
    }
  }
  puts("PASS: slow rotation with 1..250 ms event gaps at every nonzero deadzone, both directions, stop/restart");
}
