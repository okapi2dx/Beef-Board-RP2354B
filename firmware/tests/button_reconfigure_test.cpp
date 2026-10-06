#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint32_t ticks;
extern "C" uint32_t button_time_100us_now(void) { return ticks; }
#include "../at90usb1286/debounce.h"
struct config {
  uint8_t reverse_tt;
  struct { uint8_t button_debounce[7]; } iidx_keys;
  struct { uint8_t button_release_debounce[7]; } sdvx_keys;
  uint8_t iidx_effectors_debounce;
};
static Debouncer<7> buttons_debounce;
static Debouncer<4> effectors_debounce;
static void update_tt_transitions(uint8_t) {}
#include "at90_button_reconfigure_under_test.h"
int main() {
  config c = {};
  for (unsigned i=0;i<7;i++) {
    c.iidx_keys.button_debounce[i]=50;
    c.sdvx_keys.button_release_debounce[i]=50;
  }
  c.iidx_effectors_debounce=5;
  for (unsigned bit=0;bit<7;bit++) {
    const uint16_t mask=1u<<bit;
    buttons_debounce.init(c.iidx_keys.button_debounce,c.sdvx_keys.button_release_debounce,7);
    ticks=1000; assert(buttons_debounce.debounce(mask)==mask);
    ticks=1010; assert(buttons_debounce.debounce(0)==mask);
    reconfigure(c); // Saving an unrelated setting must not end the press guard.
    assert(buttons_debounce.debounce(0)==mask);
    ticks=1050; assert(buttons_debounce.debounce(0)==0);
    ticks=1060; assert(buttons_debounce.debounce(mask)==0);
    reconfigure(c); // Nor restart/end the release guard.
    assert(buttons_debounce.debounce(mask)==0);
    ticks=1100; assert(buttons_debounce.debounce(mask)==mask);
  }
  effectors_debounce.init(5);
  ticks=2000; assert(effectors_debounce.debounce(1)==1);
  ticks=2010; assert(effectors_debounce.debounce(0)==1);
  reconfigure(c); assert(effectors_debounce.debounce(0)==1);
  ticks=2050; assert(effectors_debounce.debounce(0)==0);
  // Updating B2's window must not clear B1's active guard.
  buttons_debounce.init(c.iidx_keys.button_debounce,c.sdvx_keys.button_release_debounce,7);
  ticks=3000; assert(buttons_debounce.debounce(3)==3);
  ticks=3010; assert(buttons_debounce.debounce(0)==3);
  c.iidx_keys.button_debounce[1]=0;
  reconfigure(c); assert(buttons_debounce.debounce(0)==1);
  ticks=3050; assert(buttons_debounce.debounce(0)==0);
  puts("PASS button reconfiguration: production AT90 save preserves unchanged guards; per-button edits only reset that button");
}
