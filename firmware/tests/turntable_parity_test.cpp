#include "../at90usb1286/turntable_gate.h"
#include "../at90usb1286/turntable_direction.h"
#include "../at90usb1286/analog_turntable.h"
extern "C" {
#include "../src/beef_runtime.h"
}
#include <assert.h>
#include <stdio.h>
int main() {
 for(unsigned angle=0;angle<=255;angle++)for(unsigned spacing=1;spacing<=250;spacing+=249)for(int sign=-1;sign<=1;sign+=2)for(unsigned hold=0;hold<=255;hold+=85)for(unsigned ratio=1;ratio<=10;ratio++){
  uint8_t cfg[139]={0};cfg[3]=angle;cfg[30]=ratio;cfg[85]=1;cfg[34]=hold;
  beef_turntable_t rp={};beef_turntable_update(&rp,cfg,0,0xfffffff0u);
  TurntableGate at;at.reset(127,127,0xfffffff0u);
  TurntableDirection direction;direction.reset(127,0xfffffff0u);
  AnalogTurntable analog;analog.reset(127,0xfffffff0u);
  uint8_t raw=127;uint32_t now=0xfffffff0u;
  const unsigned segment=angle+25;
  for(unsigned i=0;i<segment*4;i++){
   int step=i<segment?sign:i<segment*2?-sign:i<segment*3?0:sign;
   now+=spacing;raw=static_cast<uint8_t>(raw+step);
   uint8_t expected=at.update_steps(step,now,angle,ratio);
   beef_turntable_update(&rp,cfg,step,now);
   assert(rp.filtered==expected);
   assert(rp.output.position==analog.update(expected,now,1,hold,0,!at.reversing()));
   assert(rp.output.direction==direction.update_steps(at.accepted_steps(),now,hold));
  }
 }
 puts("PASS AT90/RP turntable parity: every encoder threshold, sensitivity 1..10, rates 1/250 ms, reversal, restarts and timer wrap");
}
