#include <stdint.h>
#include <cassert>
uint32_t milliseconds;
#include "../debounce.h"
int main() {
  Debouncer<11> d;
  d.init(5);
  milliseconds = 100;
  assert(d.debounce(1) == 1); // First press is immediate.
  milliseconds = 101;
  assert(d.debounce(0) == 1); // Bounce release is held.
  milliseconds = 102;
  assert(d.debounce(1) == 1); // Re-press does not lose the hold.
  assert(d.debounce(3) == 3); // Another button is independent.
  milliseconds = 103;
  assert(d.debounce(2) == 3);
  milliseconds = 104;
  assert(d.debounce(2) == 3);
  milliseconds = 105;
  assert(d.debounce(2) == 2); // Short tap released at window expiry.
  milliseconds = 107;
  assert(d.debounce(0) == 0); // Later release is immediate.

  d.init(5);
  milliseconds = UINT32_MAX - 1;
  assert(d.debounce(1) == 1);
  milliseconds = 0;
  assert(d.debounce(0) == 1);
  milliseconds = 3;
  assert(d.debounce(0) == 0); // Timer wrap is safe.

  d.init(0);
  assert(d.debounce(1) == 1);
  assert(d.debounce(0) == 0);
  assert(d.debounce(1) == 1);
  d.init(5);
  assert(d.debounce(0x8001, 0x7ff) == 0x8001);
}
