#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "../at90usb1286/turntable_gate.h"
#include "../at90usb1286/turntable_direction.h"
#include "../at90usb1286/analog_turntable.h"
#include "../at90usb1286/turntable_delay.h"
struct config { uint8_t tt_deadzone,tt_sustain_ms,tt_delay_ms; uint16_t milli; };
static uint16_t turntable_multiplier(const config &c){return c.milli;}
static uint32_t clock_ms;
static uint32_t current_milliseconds(){return clock_ms;}
static struct {int32_t step; void poll(){} int32_t last_step(){return step;} uint8_t get(){return 127;} uint32_t transitions(){return 0;} uint16_t last_interval(){return 0;}} tt_x;
static struct {int8_t direction;} button_x;
static TurntableGate turntable_gate;
static TurntableDirection turntable_direction;
static AnalogTurntable analog_turntable;
static TurntableDelay turntable_delay;
static uint8_t delayed_position,PINF;
static int8_t latest_tt_report;
static uint16_t button_state;
static struct {uint16_t buttons;} led_data;
static uint16_t joystick_out_state;
static void HID_Task(decltype(led_data)&,uint16_t&){}
static void process_buttons(int8_t direction){button_state=direction<0?1u<<11:direction>0?1u<<12:0;}
static void update_button_lighting(uint16_t){}
static struct {uint16_t buttons;uint8_t sensor_ab,raw_position,filtered_position,output_position;int8_t direction;uint32_t transitions;uint16_t last_transition_ms;} diagnostics;
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(x) for(bool once=true;once;once=false)
#include "at90_update_under_test.h"
int main(){
 for(unsigned milli: {0u,1u,500u,1000u,2000u}) for(unsigned gate=0;gate<=50;gate++) for(int sign=-1;sign<=1;sign+=2){
  config c={uint8_t(gate),125,0,uint16_t(milli)};
  clock_ms=0;button_x.direction=0;
  turntable_gate.reset(127,127,0);turntable_direction.reset(127,0);
  analog_turntable.reset(127,0);turntable_delay.reset(0,{127,0},0);
  for(unsigned i=1;i<=gate+2;i++){
   clock_ms=i;tt_x.step=sign;firmware_update(c);
   assert(button_x.direction==(milli && i>gate?sign:0));
  }
  tt_x.step=0;clock_ms++;firmware_update(c);
  assert(button_x.direction==0); // Holding input must not keep changing HSV.
  assert(diagnostics.direction==(milli?sign:0));
  clock_ms+=125;firmware_update(c);
  assert(button_x.direction==0 && diagnostics.direction==0);
 }
 puts("PASS production AT90 update: hardware HSV direction follows qualified encoder steps; zero/hold/release and gates 0..50");
}
