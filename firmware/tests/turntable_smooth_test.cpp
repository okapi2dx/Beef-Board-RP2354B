#include "../at90usb1286/turntable_gate.h"
#include "../at90usb1286/analog_turntable.h"
#include "../at90usb1286/turntable_direction.h"
extern "C" {
#include "../src/beef_runtime.h"
}
#include <assert.h>
#include <stdio.h>
struct Fixture {
 uint8_t cfg[139]={};beef_turntable_t rp={};TurntableGate at;
 AnalogTurntable axis;TurntableDirection direction;uint8_t raw=127;uint32_t now=0;
 Fixture(unsigned angle,unsigned hold=0){cfg[3]=angle;cfg[30]=6;cfg[34]=hold;cfg[85]=1;
  beef_turntable_update(&rp,cfg,0,0);at.reset(127,127,0);axis.reset(127,0);direction.reset(127,0);}
 uint8_t step(int delta,unsigned gap=10){now+=gap;raw=static_cast<uint8_t>(raw+delta);
  uint8_t filtered=at.update_steps(delta,now,cfg[3],cfg[30]);beef_turntable_update(&rp,cfg,delta,now);
  assert(rp.filtered==filtered);
  assert(rp.output.position==axis.update(filtered,now,1,cfg[34],0,!at.reversing()));
  assert(rp.output.direction==direction.update_steps(at.accepted_steps(),now,cfg[34]));return filtered;}
};
int main(){
 const unsigned units[11]={0,2,4,6,8,10,12,15,18,21,24};
 for(unsigned angle=1;angle<=255;angle++){
  unsigned threshold=angle+1;
  // Oscillation alone must never qualify a gesture.
  Fixture noise(angle);for(unsigned i=0;threshold>1 && i<100;i++){assert(noise.step(1)==127);assert(noise.step(-1)==127);}
  Fixture f(angle);for(unsigned i=1;i<threshold;i++)assert(f.step(1)==127);
  uint8_t position=128;assert(f.step(1)==position);
  // A tiny reversal is absorbed; returning movement cancels its distance.
  for(unsigned i=0;threshold>1 && i<100;i++){
   assert(f.step(-1)==position);assert(f.step(1)==position);
   position++;assert(f.step(1,i%2?60:200)==position);
  }
  // Actual reversal qualifies once and then follows every subsequent count.
  for(unsigned i=1;i<threshold;i++)assert(f.step(-1)==position);
  position--;assert(f.step(-1)==position);
  for(unsigned i=0;i<30;i++){position--;assert(f.step(-1,i%2?15:180)==position);}
  // Blocking must not produce a catch-up jump for batched encoder events.
  Fixture batch(angle);assert(batch.step(threshold+3)==131);
  // A deliberate pause starts a new angle-gated gesture.
  assert(f.step(1,2000)==static_cast<uint8_t>(position+(threshold<=1?1:0)));
 }
 // Qualification uses physical encoder counts, regardless of sensitivity.
 for(unsigned sensitivity=1;sensitivity<=10;sensitivity++)for(int sign=-1;sign<=1;sign+=2){
  uint8_t cfg[139]={};cfg[3]=5;cfg[30]=sensitivity;
  beef_turntable_t rp={};beef_turntable_update(&rp,cfg,0,0);
  TurntableGate at;at.reset(127,127,0);
  for(unsigned i=1;i<=25;i++){
   const uint8_t expected=at.update_steps(sign,i,5,sensitivity);
   beef_turntable_update(&rp,cfg,sign,i);
   assert(rp.filtered==expected);
   assert(rp.output.direction==(i<=5?0:sign));
   const unsigned accepted=i<=5?0:i-5;
   assert(rp.filtered==static_cast<uint8_t>(127+sign*(int)(accepted*units[sensitivity]/12)));
  }
 }
 Fixture zero(0);assert(zero.step(1)==128);assert(zero.step(-1)==127);
 // Reverse-gating must stop the previous direction's synthetic analog hold.
 Fixture hold(5,100);for(unsigned i=0;i<6;i++)hold.step(1);
 uint8_t before=hold.rp.output.position;hold.step(-1);
 for(unsigned i=0;i<50;i++)hold.step(0,1);
 assert(hold.rp.output.position==before);
 puts("PASS smooth scratch: jitter, net encoder displacement, uneven spacing, gated reversal, no catch-up jump, pause, zero and reverse-hold cancellation");
}
