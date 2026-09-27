#include "../turntable_delay.h"
#include <assert.h>
#include <stdio.h>

TurntableSample input(uint32_t tick) {
  return {static_cast<uint8_t>(tick), static_cast<int8_t>(tick % 3 - 1)};
}
int main() {
  for (int delay = 0; delay <= 255; ++delay) {
    TurntableDelay line;
    const uint32_t start = 0xffffff00UL;
    line.reset(start, {0, 0}, delay);
    for (uint32_t tick = 1; tick <= 1024; ++tick) {
      const auto result = line.update(start + tick, input(tick), delay);
      const auto expected = tick <= static_cast<uint32_t>(delay) ? TurntableSample{0, 0} : input(tick - delay);
      assert(result.position == expected.position);
      assert(result.direction == expected.direction);
    }
  }
  TurntableDelay line;
  line.reset(0, {10, 0}, 100);
  line.update(20, {20, 1}, 100);
  assert(line.update(119, {20, 1}, 100).position == 10);
  assert(line.update(120, {20, 1}, 100).direction == 1);
  line.update(121, {20, 0}, 100);
  assert(line.update(220, {20, 0}, 100).direction == 1);
  assert(line.update(221, {20, 0}, 100).direction == 0);
  assert(line.update(1000, {30, -1}, 100).position == 20);
  assert(line.update(1100, {30, -1}, 100).position == 30);
  assert(line.update(1101, {40, 1}, 0).position == 40);
  assert(line.update(1102, {50, -1}, 255).position == 40);
  assert(line.update(1357, {50, -1}, 255).position == 50);
  puts("PASS: delays 0..255, position/direction, release timing, timer rollover, gaps and delay changes");
}
