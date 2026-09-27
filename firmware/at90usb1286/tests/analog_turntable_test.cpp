#include "../analog_turntable.h"
#include <assert.h>
#include <stdio.h>

int main() {
  AnalogTurntable axis;

  axis.reset(0, 0);
  assert(axis.update(1, 10, 1, 0, 0) == 1);
  assert(axis.update(1, 200, 1, 0, 0) == 1);

  axis.reset(0, 0);
  assert(axis.update(1, 10, 1, 100, 1) == 1);
  assert(axis.update(1, 19, 1, 100, 1) == 1);
  assert(axis.update(1, 20, 1, 100, 1) == 2);
  assert(axis.update(1, 110, 1, 100, 1) == 11);
  assert(axis.update(1, 200, 1, 100, 1) == 11);

  axis.reset(100, 0);
  assert(axis.update(99, 10, 1, 100, 1) == 99);
  assert(axis.update(99, 110, 1, 100, 1) == 89);

  axis.reset(0, 0);
  assert(axis.update(1, 10, 1, 255, 1) == 1);
  assert(axis.update(1, 265, 1, 255, 1) == 26);

  puts("PASS: fixed 100 counts/sec X hold, independent of previous speed");
}
