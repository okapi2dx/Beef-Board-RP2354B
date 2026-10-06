#include "../at90usb1286/turntable_gate.h"
#include "../at90usb1286/turntable_direction.h"
#include "../at90usb1286/analog_turntable.h"
#include "../at90usb1286/turntable_delay.h"
extern "C" {
#include "../src/beef_runtime.h"
}
#include <assert.h>
#include <stdio.h>
struct config { uint8_t reverse_tt,tt_deadzone,tt_ratio,tt_sustain_ms,digital_tt,tt_delay_ms; };
static uint16_t turntable_multiplier(const config &c) { return beef_sensitivity_milli(c.tt_ratio); }
static config current_config;
static uint32_t now;
static uint32_t current_milliseconds(){return now;}
static struct {uint8_t get()const{return 127;}} tt_x;
static uint8_t delayed_position;
static TurntableGate turntable_gate;
static AnalogTurntable analog_turntable;
static TurntableDirection turntable_direction;
static TurntableDelay turntable_delay;
#include "at90_reconfigure_under_test.h"
static void encode(uint8_t *c,const config &value){c[1]=value.reverse_tt;c[3]=value.tt_deadzone;c[30]=value.tt_ratio;c[34]=value.tt_sustain_ms;c[85]=value.digital_tt;c[86]=value.tt_delay_ms;}
int main(){
 for(unsigned setting=0;setting<6;setting++)for(unsigned delay=0;delay<=255;delay++){
  now=0;current_config={0,0,10,50,1,0};delayed_position=127;
  turntable_gate.reset(127,127,now);analog_turntable.reset(127,now);
  turntable_direction.reset(127,now);turntable_delay.reset(now,{127,0},0);
  uint8_t c[139]={};encode(c,current_config);beef_turntable_t rp={};beef_turntable_update(&rp,c,0,now);
  for(unsigned time=1;time<=30;time++){
   now=time;const int step=time<11?1:0;
   const uint8_t filtered=turntable_gate.update_steps(step,now,current_config.tt_deadzone,current_config.tt_ratio);
   const auto direction=turntable_direction.update_steps(turntable_gate.accepted_steps(),now,current_config.tt_sustain_ms);
   const auto axis=analog_turntable.update(filtered,now,1,current_config.tt_sustain_ms,0,!turntable_gate.reversing());
   auto output=turntable_delay.update(now,{axis,direction},current_config.tt_delay_ms);delayed_position=output.position;
   beef_turntable_update(&rp,c,step,now);assert(output.position==rp.output.position && output.direction==rp.output.direction);
  }
  config next=current_config;
  if(setting==0)next.reverse_tt=1;
  if(setting==1)next.tt_deadzone=5;
  if(setting==2)next.tt_ratio=5;
  if(setting==3)next.tt_sustain_ms=100;
  if(setting==4)next.digital_tt=0;
  if(setting==5)next.tt_delay_ms=delay;
  now=31;at90_reconfigure(next);current_config=next;encode(c,current_config);
  for(unsigned time=31;time<=350;time++){
   now=time;int raw=time>=40 && time<70?1:0;int step=current_config.reverse_tt?-raw:raw;
   const uint8_t filtered=turntable_gate.update_steps(step,now,current_config.tt_deadzone,current_config.tt_ratio);
   const auto direction=turntable_direction.update_steps(turntable_gate.accepted_steps(),now,current_config.tt_sustain_ms);
   const auto axis=analog_turntable.update(filtered,now,1,current_config.tt_sustain_ms,0,!turntable_gate.reversing());
   auto output=turntable_delay.update(now,{axis,direction},current_config.tt_delay_ms);delayed_position=output.position;
   beef_turntable_update(&rp,c,raw,now);assert(output.position==rp.output.position && output.direction==rp.output.direction);
  }
 }
 puts("PASS live reconfiguration: production AT90 reset logic vs RP, reversal/count/sensitivity/hold/mode and delays 0..255");
}
