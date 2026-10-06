#include <cassert>
#include <cstdint>
#include <cstdio>
static uint32_t ticks_100us;
extern "C" uint32_t button_time_100us_now(void) { return ticks_100us; }
#include "../at90usb1286/debounce.h"
static void check(unsigned button, uint8_t p, uint8_t r, bool original_case) {
  Debouncer<11> at90;
  beef_button_debouncer_t rp;
  uint8_t press[11] = {}, release[11] = {};
  press[button] = p; release[button] = r;
  at90.init(press, release, 11); beef_button_debouncer_init(&rp, 0);
  uint16_t bit = uint16_t(1u << button);
  bool expected = false;
  unsigned unlocked = 0;
  unsigned total = original_case ? 10200 : 800;
  for (unsigned offset = 0; offset <= total; ++offset) {
    bool raw = original_case ? offset < 20 || (offset >= 30 && offset < 10030)
      : offset < 16 || (offset >= 27 && offset < 527) || (offset >= 547 && offset < 577);
    if (offset >= unlocked && raw != expected) {
      expected = raw;
      unlocked = offset + (expected ? p : r);
    }
    ticks_100us = 1000 + offset;
    beef_button_debouncer_edge(&rp, button, raw, ticks_100us * 100u, p, r);
    uint16_t actual = at90.debounce(raw ? bit : 0);
    assert(actual == (expected ? bit : 0)); assert(rp.accepted == actual);
    if (original_case && p == 50) {
      assert(actual == (offset < 10030 ? bit : 0)); // Exactly 1003ms, no release extension.
    }
    if (!original_case && p == 0 && r == 12) {
      bool exact = offset < 16 || (offset >= 28 && offset < 527) || (offset >= 547 && offset < 577);
      assert(actual == (exact ? bit : 0));
    }
  }
}
int main() {
  uint8_t windows[] = {0, 1, 12, 50, 160};
  for (unsigned b=0; b<7; ++b) for (uint8_t p : windows) for (uint8_t r : windows) {
    check(b,p,r,true); check(b,p,r,false);
  }
  puts("PASS AT90/RP B1-B7: 25 press/release pairs, immediate first edges, exactly 1003ms, sustained recovery");
}
