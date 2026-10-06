#include "../at90usb1286/turntable_gate.h"
extern "C" {
#include "../src/beef_runtime.h"
}
#include <assert.h>
#include <stdio.h>
int main() {
 for (unsigned milli=0;milli<=2000;milli++) for (unsigned gate=0;gate<=50;gate+=5) for (int sign=-1;sign<=1;sign+=2) {
  uint8_t c[139]={};c[0]=45;c[30]=6;c[3]=gate;c[71]=milli;c[72]=milli>>8;
  beef_turntable_t rp={};beef_turntable_update(&rp,c,0,0);
  TurntableGate at;at.reset(127,127,0);
  for(unsigned i=1;i<=100;i++) {
   uint8_t out=at.update_milli(sign,i,gate,milli);
   beef_turntable_update(&rp,c,sign,i);
   unsigned accepted=i>gate?i-gate:0;
   uint8_t expected=static_cast<uint8_t>(127+sign*int(accepted*milli/1000));
   if(out!=expected || rp.filtered!=expected || rp.output.position!=expected) { fprintf(stderr,"m=%u gate=%u sign=%d i=%u at=%u rp=%u pos=%u expected=%u\n",milli,gate,sign,i,out,rp.filtered,rp.output.position,expected); }
   assert(out==expected && rp.filtered==expected && rp.output.position==expected);
   assert(rp.output.direction==(milli && accepted?sign:0));
  }
 }
 puts("PASS multiplier: all 0..2000 values, positive/negative, raw-count deadzone, fractional carry, zero disabled, AT90/RP parity");
}
