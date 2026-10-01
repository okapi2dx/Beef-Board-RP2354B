#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "button_debounce.h"

int main(void) {
  beef_button_debouncer_t state;
  beef_button_debouncer_init(&state, 0);
  beef_button_debouncer_edge(&state, 0, true, 1000, 5, 5);
  assert(state.accepted == 1); // Initial ON is immediate.
  beef_button_debouncer_edge(&state, 0, false, 1100, 5, 5);
  assert(state.accepted == 1); // Brief OFF is rejected.
  beef_button_debouncer_edge(&state, 0, true, 1200, 5, 5);
  assert(state.accepted == 1); // Return ON is not a second press.
  beef_button_debouncer_edge(&state, 0, false, 1600, 5, 5);
  assert(state.accepted == 0); // Initial release is immediate.
  beef_button_debouncer_edge(&state, 0, true, 1700, 5, 5);
  assert(state.accepted == 0); // Brief ON is rejected.
  beef_button_debouncer_edge(&state, 0, false, 1800, 5, 5);
  assert(state.accepted == 0); // Return OFF is not a second release.
  beef_button_debouncer_edge(&state, 0, true, 2100, 5, 5);
  assert(state.accepted == 1); // At 0.5 ms, a new press is accepted.

  beef_button_debouncer_init(&state, 1);
  beef_button_debouncer_edge(&state, 0, false, 1000, 5, 5);
  assert(state.accepted == 0);
  beef_button_debouncer_edge(&state, 0, true, 1400, 5, 5);
  beef_button_debouncer_tick(&state, 1500);
  assert(state.accepted == 0); // A 0.4 ms ON bounce stays ignored at expiry.
  beef_button_debouncer_edge(&state, 0, false, 1900, 5, 5);
  assert(state.accepted == 0); // Its 0.9 ms OFF return adds no release.

  beef_button_debouncer_init(&state, 0);
  beef_button_debouncer_edge(&state, 0, true, 1000, 5, 5);
  beef_button_debouncer_edge(&state, 0, false, 1100, 5, 5);
  beef_button_debouncer_tick(&state, 1499);
  assert(state.accepted == 1);
  beef_button_debouncer_tick(&state, 1500);
  assert(state.accepted == 0); // A short tap eventually releases.

  beef_button_debouncer_init(&state, 0);
  beef_button_debouncer_edge(&state, 0, true, UINT32_MAX - 200, 5, 5);
  beef_button_debouncer_edge(&state, 0, false, 100, 5, 5);
  assert(state.accepted == 1);
  beef_button_debouncer_tick(&state, 300);
  assert(state.accepted == 0); // Clock wrap is safe.

  beef_button_debouncer_init(&state, 0);
  beef_button_debouncer_edge(&state, 0, true, 1000, 0, 0);
  beef_button_debouncer_edge(&state, 0, false, 1010, 0, 0);
  beef_button_debouncer_edge(&state, 0, true, 1020, 0, 0);
  assert(state.accepted == 1); // Zero passes every edge.
  puts("PASS button debounce: immediate operations, opposite-edge guards, short tap, wrap, zero");
  return 0;
}
