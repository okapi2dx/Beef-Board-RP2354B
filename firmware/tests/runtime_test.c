#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "beef_runtime.h"
#include "beef_lighting.h"
static uint8_t c[139];
static void defaults(void) {
 memset(c,0,sizeof(c)); c[0]=33;c[30]=10;c[78]=60;c[79]=3;c[80]=24;c[138]=16;c[88]=100;c[4]=8;
 for(unsigned i=0;i<11;i++){c[102+i]=100;c[127+i]=i;}
 for(unsigned i=6;i<=27;i+=3){c[i]=64;c[i+1]=255;c[i+2]=255;}
 c[81]=128;c[82]=255;c[83]=255;
}
static void input_tests(void) {
 beef_turntable_t s={0};defaults();c[34]=50;
 beef_turntable_update(&s,c,0,100);beef_turntable_update(&s,c,1,101);
 for(unsigned t=102;t<=1101;t++)beef_turntable_update(&s,c,0,t);
 assert(s.output.direction==0 && s.output.position==133);
 uint8_t end=s.output.position;beef_turntable_update(&s,c,0,2101);assert(s.output.position==end);
 // Deadzone: 10 degrees rounds up to 8 axis counts; reversal gates again.
 memset(&s,0,sizeof(s));c[34]=0;c[3]=10;beef_turntable_update(&s,c,0,0);
 for(unsigned t=1;t<8;t++){beef_turntable_update(&s,c,1,t);assert(s.output.position==127);}
 beef_turntable_update(&s,c,1,8);assert(s.output.position==128);
 beef_turntable_update(&s,c,1,9);assert(s.output.position==129);
 for(unsigned t=10;t<17;t++){beef_turntable_update(&s,c,-1,t);assert(s.output.position==129);}
 beef_turntable_update(&s,c,-1,17);assert(s.output.position==128);
 // Full delay range at maximum sampling rate, preserving press AND release.
 for(unsigned delay=0;delay<=255;delay++) {
  memset(&s,0,sizeof(s));defaults();c[85]=1;c[86]=delay;
  beef_turntable_update(&s,c,0,0);
  for(unsigned t=1;t<=300;t++) {
   beef_turntable_update(&s,c,t==1?1:0,t);
   assert(s.output.direction==(t==delay+1?1:0));
   assert(s.output.position==(t>=delay+1?128:127));
  }
 }
 memset(&s,0,sizeof(s));defaults();c[3]=30;c[85]=1;
 beef_turntable_update(&s,c,0,0);beef_turntable_update(&s,c,1,1);
 assert(s.output.direction==1 && s.output.position==127);
 // Clock wrap, reversal, sensitivity and a settings change while holding.
 memset(&s,0,sizeof(s));defaults();c[34]=50;
 beef_turntable_update(&s,c,0,0xfffffff0u);beef_turntable_update(&s,c,1,0xfffffff1u);
 for(unsigned t=0xfffffff2u;t!=60;t++)beef_turntable_update(&s,c,0,t);
 assert(s.output.direction==0 && s.output.position==133);
 memset(&s,0,sizeof(s));defaults();c[30]=5;c[1]=1;
 beef_turntable_update(&s,c,0,0);beef_turntable_update(&s,c,1,1);assert(s.raw==127);
 beef_turntable_update(&s,c,1,2);assert(s.raw==126);
 c[34]=100;beef_turntable_update(&s,c,1,3);c[85]=1;
 beef_turntable_update(&s,c,0,4);assert(s.output.direction==0);
 uint8_t keys[13];c[46]=0x51;c[47]=0x52;c[35]=0x52;
 beef_keyboard_report(c,1u<<11,keys);assert(keys[0]==0x51 && keys[1]==0);
 beef_keyboard_report(c,(1u<<12)|1u,keys);assert(keys[0]==0x52 && keys[1]==0);
 beef_keyboard_report(c,0,keys);for(unsigned i=0;i<13;i++)assert(keys[i]==0);
 puts("PASS input: bounded hold, deadzone/reversal, delays 0..255, clock wrap, sensitivity, keyboard TT and duplicate keys");
}
static void lighting_tests(void) {
 defaults();beef_lighting_t s={0};beef_rgb_t ring[255],bar[255],before[255];uint8_t host[3]={12,34,56};
 beef_lighting_update(&s,c,0,127,0,0);
 for(unsigned mode=0;mode<10;mode++) {
  c[2]=mode;c[84]=1;
  beef_lighting_render(&s,c,true,host,host,ring,bar);
  for(unsigned i=0;i<16;i++)assert(!memcmp(&bar[i],&ring[i*24/16],sizeof(bar[i])));
 }
 for(unsigned mode=1;mode<=7;mode++) {
  if(mode==3)continue;
  memset(&s,0,sizeof(s));c[2]=mode;
  beef_lighting_update(&s,c,0,127,0,0);beef_lighting_render(&s,c,false,host,host,before,bar);
  for(unsigned t=1;t<=333;t++)beef_lighting_update(&s,c,t,127+t,1,0);
  beef_lighting_render(&s,c,false,host,host,ring,bar);
  assert(memcmp(before,ring,24*sizeof(*ring))!=0);
 }
 c[2]=8;c[84]=0;c[4]=4;beef_lighting_render(&s,c,true,host,host,ring,bar);
 assert(ring[0].r==12 && bar[0].b==56);
 beef_lighting_render(&s,c,false,host,host,ring,bar);assert(ring[0].r==0 && bar[0].b==0);
 c[2]=0;c[4]=8;c[80]=255;c[138]=255;
 beef_lighting_render(&s,c,false,host,host,ring,bar);assert(ring[254].g>0 && bar[254].b>0);
 c[80]=1;c[138]=1;beef_lighting_render(&s,c,false,host,host,ring,bar);
 for(unsigned i=1;i<255;i++)assert(ring[i].r==0 && ring[i].g==0 && ring[i].b==0 && bar[i].b==0);
 c[5]=1;beef_lighting_render(&s,c,true,host,host,ring,bar);assert(ring[0].g==0 && bar[0].b==0);
 for(unsigned rate=1;rate<=60;rate++) {
  unsigned interval=(1000+rate-1)/rate;
  assert(!beef_lighting_due(100+interval-1,100,rate));assert(beef_lighting_due(100+interval,100,rate));
 }
 c[5]=0;c[4]=1;memset(&s,0,sizeof(s));beef_lighting_update(&s,c,0,127,0,0);
 beef_lighting_update(&s,c,1,127,0,1);beef_lighting_render(&s,c,false,host,host,ring,bar);assert(s.level==1);
 beef_lighting_update(&s,c,1000,127,0,1);assert(s.level==0);
 puts("PASS lighting: moving effects, linked pixels, HID timeout, 255 pixels, tail clearing, disable, 1..60 Hz and key spectrum decay");
}
int main(void){input_tests();lighting_tests();defaults();assert(beef_config_valid(c));c[80]=0;assert(!beef_config_valid(c));return 0;}
