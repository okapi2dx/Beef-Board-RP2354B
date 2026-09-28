#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "button_debounce.h"

int main(void) {
  beef_button_debouncer_t state;
  beef_button_debouncer_init(&state, 0);

  beef_button_debouncer_edge(&state, 0, true, 1000, 5);
  assert(state.accepted == 1); // First press has no debounce delay.
  beef_button_debouncer_edge(&state, 0, false, 1100, 5);
  assert(state.accepted == 1); // Early release may be contact bounce.
  beef_button_debouncer_edge(&state, 0, true, 1200, 5);
  assert(state.accepted == 1); // Bounce must not lose a continuing hold.
  beef_button_debouncer_edge(&state, 1, true, 1300, 5);
  assert(state.accepted == 3); // Buttons have independent cooldowns.
  beef_button_debouncer_edge(&state, 0, false, 1400, 5);
  beef_button_debouncer_tick(&state, 5999, 5);
  assert(state.accepted == 3);
  beef_button_debouncer_tick(&state, 6000, 5);
  assert(state.accepted == 2); // A short tap is released at window expiry.
  beef_button_debouncer_edge(&state, 0, true, 6000, 5);
  assert(state.accepted == 3);

  beef_button_debouncer_edge(&state, 0, false, 11000, 5);
  assert(state.accepted == 2); // Later releases are immediate.

  beef_button_debouncer_edge(&state, 0, false, 6100, 0);
  beef_button_debouncer_edge(&state, 0, true, 6200, 0);
  assert(state.accepted == 3); // 0 ms passes every edge.

  beef_button_debouncer_init(&state, 0);
  beef_button_debouncer_edge(&state, 0, true, UINT32_MAX - 500, 2);
  beef_button_debouncer_edge(&state, 0, false, UINT32_MAX - 400, 2);
  beef_button_debouncer_edge(&state, 0, true, 1000, 2);
  assert(state.accepted == 1); // Microsecond counter wrap is safe.
  beef_button_debouncer_edge(&state, 0, false, 1200, 2);
  beef_button_debouncer_tick(&state, 1498, 2);
  assert(state.accepted == 1);
  beef_button_debouncer_tick(&state, 1499, 2);
  assert(state.accepted == 0);
  beef_button_debouncer_edge(&state, 0, true, 1600, 2);
  assert(state.accepted == 1);

  puts("PASS button debounce: immediate press, bounce hold, timed release, zero window, timer wrap");
  return 0;
}
