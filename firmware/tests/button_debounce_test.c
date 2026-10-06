#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "button_debounce.h"
int main(void) {
  beef_button_debouncer_t s;
  beef_button_debouncer_init(&s, 0);
  beef_button_debouncer_edge(&s, 0, true, 1000, 50, 12);
  assert(s.accepted == 1);
  beef_button_debouncer_edge(&s, 0, false, 3000, 50, 12);
  assert(s.accepted == 1);
  beef_button_debouncer_edge(&s, 0, true, 4000, 50, 12);
  beef_button_debouncer_edge(&s, 0, false, 1004000, 50, 12);
  assert(s.accepted == 0); // Final release is immediate, regardless of release setting.
  beef_button_debouncer_edge(&s, 0, true, 1005100, 50, 12);
  assert(s.accepted == 0);
  beef_button_debouncer_tick(&s, 1005199); assert(s.accepted == 0);
  beef_button_debouncer_tick(&s, 1005200); assert(s.accepted == 1); // Sustained ON recovered.
  beef_button_debouncer_init(&s, 0);
  beef_button_debouncer_edge(&s, 0, true, UINT32_MAX - 200, 5, 12);
  beef_button_debouncer_edge(&s, 0, false, 100, 5, 12);
  assert(s.accepted == 1);
  beef_button_debouncer_tick(&s, 300); assert(s.accepted == 0);
  beef_button_debouncer_init(&s, 0);
  beef_button_debouncer_edge(&s, 0, true, 1000, 0, 0);
  beef_button_debouncer_edge(&s, 0, false, 1100, 0, 0);
  assert(s.accepted == 0);
  puts("PASS debounce: immediate first press/release, reverse lockout, sustained recovery, wrap, zero");
}
