#include <stdint.h>
#include <cassert>
static uint32_t ticks_100us;
extern "C" uint32_t button_time_100us_now(void) { return ticks_100us; }
#include "../debounce.h"

int main() {
  Debouncer<11> d;
  uint8_t press[11] = {5}, release[11] = {5};
  d.init(press, release, 11);
  ticks_100us = 1000;
  assert(d.debounce(1) == 1);
  ticks_100us = 1001;
  assert(d.debounce(0) == 1); // Brief OFF after press is ignored.
  ticks_100us = 1002;
  assert(d.debounce(1) == 1);
  ticks_100us = 1005;
  assert(d.debounce(0) == 0); // Release is immediate at guard boundary.
  ticks_100us = 1006;
  assert(d.debounce(1) == 0); // Brief ON after release is ignored.
  ticks_100us = 1007;
  assert(d.debounce(0) == 0);
  ticks_100us = 1010;
  assert(d.debounce(1) == 1);

  d.init(press, release, 11);
  ticks_100us = 1200;
  assert(d.debounce(1) == 1);
  ticks_100us = 1205;
  assert(d.debounce(0) == 0);
  ticks_100us = 1209;
  assert(d.debounce(1) == 0); // 0.4 ms bounce begins inside release guard.
  ticks_100us = 1210;
  assert(d.debounce(1) == 0); // Guard expiry cannot resurrect that ON pulse.
  ticks_100us = 1214;
  assert(d.debounce(0) == 0);

  d.init(press, release, 11);
  ticks_100us = 2000;
  assert(d.debounce(1) == 1);
  ticks_100us = 2001;
  assert(d.debounce(0) == 1);
  ticks_100us = 2005;
  assert(d.debounce(0) == 0); // Held OFF is released at press guard expiry.

  d.init(5);
  ticks_100us = UINT32_MAX - 1;
  assert(d.debounce(1) == 1);
  ticks_100us = 0;
  assert(d.debounce(0) == 1);
  ticks_100us = 49;
  assert(d.debounce(0) == 0); // Timer wrap is safe.

  d.init(0);
  assert(d.debounce(1) == 1);
  assert(d.debounce(0) == 0);
  assert(d.debounce(1) == 1);
  d.init(5);
  assert(d.debounce(0x8001, 0x7ff) == 0x8001);
}
