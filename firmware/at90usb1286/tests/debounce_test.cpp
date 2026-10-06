#include <stdint.h>
#include <cassert>
static uint32_t ticks_100us;
extern "C" uint32_t button_time_100us_now(void) { return ticks_100us; }
#include "../debounce.h"
int main() {
  Debouncer<11> d;
  uint8_t press[11] = {}, release[11] = {12};
  d.init(press, release, 11);
  ticks_100us = 1000; assert(d.debounce(1) == 1);
  ticks_100us = 1016; assert(d.debounce(0) == 0);
  ticks_100us = 1027; assert(d.debounce(1) == 0);
  ticks_100us = 1028; assert(d.debounce(1) == 1);
  ticks_100us = 1527; assert(d.debounce(0) == 0);
  ticks_100us = 1547; assert(d.debounce(1) == 1);
  d.init(5);
  ticks_100us = UINT32_MAX - 1; assert(d.debounce(1) == 1);
  ticks_100us = 0; assert(d.debounce(0) == 1);
  ticks_100us = 50; assert(d.debounce(0) == 0);
  d.init(0); assert(d.debounce(1) == 1); assert(d.debounce(0) == 0);
  assert(d.debounce(0x8001, 0x7ff) == 0x8001);
}
