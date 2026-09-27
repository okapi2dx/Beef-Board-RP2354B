#include "../button_led_envelope.h"
#include <cassert>
#include <cstdio>
int main() {
  ButtonLedEnvelope led;
  assert(led.update(false, 0, 255, 100) == 0);
  assert(led.update(true, 1, 100, 100) == 100);
  assert(led.update(false, 20, 100, 100) == 100);
  assert(led.update(false, 70, 100, 100) == 50);
  assert(led.update(false, 120, 100, 100) == 0);
  assert(led.update(true, 130, 255, 35) == 35);
  assert(led.update(false, 131, 0, 35) == 0);
  for (int fade = 1; fade <= 255; ++fade) {
    led.reset();
    led.update(true, 0, fade, 100);
    led.update(false, 1, fade, 100);
    int previous = 100;
    for (int t = 0; t <= fade; ++t) {
      const auto value = led.update(false, t + 1, fade, 100);
      assert(value <= previous);
      previous = value;
    }
    assert(previous == 0);
  }
  ButtonLedEnvelope long_fade;
  long_fade.update(true, 0, 1000, 100);
  assert(long_fade.update(false, 1, 1000, 100) == 100);
  assert(long_fade.update(false, 501, 1000, 100) == 50);
  assert(long_fade.update(false, 1001, 1000, 100) == 0);
  led.update(true, 0xfffffff0UL, 255, 100);
  led.update(false, 0xfffffff0UL, 255, 100);
  assert(led.update(false, 0x000000efUL, 255, 100) == 0);
  led.update(true, 100, 255, 100);
  led.update(false, 101, 255, 100);
  assert(led.update(true, 102, 255, 75) == 75);
  assert(led.update(true, 103, 255, 0) == 0);
  led.reset();
  assert(led.update(false, 104, 255, 100) == 0);
  puts("Button LED fade tests passed");
}
