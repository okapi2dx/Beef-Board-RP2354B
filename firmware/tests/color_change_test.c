#include <assert.h>
#include <stdio.h>
#include "color_change.h"

int main(void) {
  for (uint8_t speed = 1; speed <= 5; speed++) {
    beef_color_shift_t shift = {0};
    beef_breathing_t breath = {0};
    assert(beef_color_shift_update(&shift, 100, speed) == 0);
    assert(beef_breathing_update(&breath, 100, speed) == 0);
    assert(beef_color_shift_update(&shift, 400, speed) == speed);
    assert(beef_breathing_update(&breath, 400, speed) == 100 * speed);
    assert(beef_breathing_update(&breath, 100 + 9000 / speed, speed) == 0);
  }
  beef_color_shift_t shift = {0};
  beef_breathing_t breath = {0};
  beef_color_shift_update(&shift, UINT32_MAX - 99, 3);
  beef_breathing_update(&breath, UINT32_MAX - 99, 3);
  assert(beef_color_shift_update(&shift, 0, 3) == 1);
  assert(beef_breathing_update(&breath, 0, 3) == 100);
  assert(beef_color_shift_update(&shift, 0, 5) == 1);
  assert(beef_breathing_update(&breath, 0, 5) == 100);
  assert(beef_color_shift_update(&shift, 60, 5) == 2);
  assert(beef_breathing_update(&breath, 60, 5) == 200);
  puts("PASS color change: five speeds, breathing cycles, clock wrap and continuous speed edits");
}
