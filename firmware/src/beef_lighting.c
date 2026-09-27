#include "beef_lighting.h"
#include <math.h>
#include <string.h>
static float wrap(float x, float n) { x=fmodf(x,n); return x<0 ? x+n : x; }
static beef_rgb_t hsv(uint8_t h,uint8_t s,uint8_t v) {
  if (!s) return (beef_rgb_t){v,v,v};
  unsigned region=h/43, remainder=(h-region*43)*6;
  uint8_t p=(v*(255u-s))>>8, q=(v*(255u-((s*remainder)>>8)))>>8;
  uint8_t t=(v*(255u-((s*(255u-remainder))>>8)))>>8;
  switch(region) {
    case 0:return (beef_rgb_t){v,t,p}; case 1:return (beef_rgb_t){q,v,p};
    case 2:return (beef_rgb_t){p,v,t}; case 3:return (beef_rgb_t){p,q,v};
    case 4:return (beef_rgb_t){t,p,v}; default:return (beef_rgb_t){v,p,q};
  }
}
static uint8_t breathing(uint32_t time) {
  unsigned theta=(time%3000)/8; if(theta>255)theta=255;
  float x=(theta<128 ? theta*2 : (255-theta)*2)/255.0f;
  return (uint8_t)(255*(x<0.5f ? 2*x*x : 1-2*(1-x)*(1-x)));
}
bool beef_lighting_due(uint32_t now,uint32_t last_frame,uint8_t refresh) {
  if(refresh<1)refresh=1;
  if(refresh>60)refresh=60;
  return (uint32_t)(now-last_frame)>=(1000u+refresh-1)/refresh;
}
void beef_lighting_update(beef_lighting_t *s,const uint8_t *c,uint32_t now,
                          uint8_t position,int8_t direction,uint16_t buttons) {
  if(!s->initialized) {
    memset(s,0,sizeof(*s)); s->last_ms=now; s->previous_position=position; s->initialized=true;
  }
  uint32_t dt=now-s->last_ms; s->elapsed_ms+=dt;
  int d=direction*(c[1]?1:-1);
  s->spin=wrap(s->spin+(float)dt/(d?25:50)*(d==-1?-1:1),12);
  s->rainbow=wrap(s->rainbow+(float)dt/(d?2:3)*(d==-1?-1:1),256);
  int delta=(int)position-s->previous_position;
  if(delta>127)delta-=256;
  if(delta< -128)delta+=256;
  s->previous_position=position;
  if(delta && dt) {
    float next=(float)delta/(float)dt*6*(c[1]?1:-1);
    if(next>1)next=1;
    if(next< -1)next=-1;
    if(next*s->velocity<0)s->velocity=0;
    s->target_velocity=next; s->last_motion=now; s->motion_seen=true;
  }
  uint32_t age=now-s->last_motion;
  if(!s->motion_seen || age>=150){s->target_velocity=0;s->velocity=0;}
  float target=age<=60 ? s->target_velocity : s->target_velocity*fmaxf(0,1-(age-60)/90.0f);
  s->velocity+=(target-s->velocity)*(1-expf(-(float)dt/40));
  s->reactive=wrap(s->reactive+dt/3.0f*s->velocity*c[79],256);
  if(d){s->guard_ms=now;s->guard_seen=true;s->reactive_hue=d==1?0:128;}
  if(buttons!=s->previous_buttons){if(s->level<16)++s->level;s->decay_ms=now;}
  else if(now-s->decay_ms>=30){
    unsigned decay=(now-s->decay_ms)/30;
    s->level=decay>=s->level?0:s->level-decay;s->decay_ms=now;
  }
  s->previous_buttons=buttons;s->last_ms=now;
}
void beef_lighting_render(const beef_lighting_t *s,const uint8_t *c,
                          bool host_active,const uint8_t tt_rgb[3],const uint8_t bar_rgb[3],
                          beef_rgb_t ring[BEEF_MAX_LEDS],beef_rgb_t bar[BEEF_MAX_LEDS]) {
  memset(ring,0,BEEF_MAX_LEDS*sizeof(*ring));memset(bar,0,BEEF_MAX_LEDS*sizeof(*bar));
  if(c[5])return;
  unsigned n=c[80],m=c[138],effect=c[2];
  unsigned offset=effect<=7 ? 6+3*effect : 6;
  uint8_t h=c[offset],sat=c[offset+1],v=c[offset+2];
  for(unsigned i=0;i<n;++i){
    switch(effect){
      case 0:ring[i]=hsv(h,sat,v);break;
      case 1:if((i*24/n)%12==(unsigned)s->spin)ring[i]=hsv(h,255,255);break;
      case 2:ring[i]=hsv((s->elapsed_ms/100)%256,sat,v);break;
      case 3:case 4:case 5:{
        float pos=effect==3?0:effect==4?s->reactive:s->rainbow*c[79];
        ring[i]=hsv((uint8_t)wrap(i*256.0f/n-pos,256),sat,v);break;
      }
      case 6:ring[i]=hsv(h+s->reactive_hue,sat,
          s->guard_seen && s->last_ms-s->guard_ms<500 ? 255:64);break;
      case 7:ring[i]=hsv(h,sat,breathing(s->elapsed_ms));break;
      case 8:if(host_active)ring[i]=(beef_rgb_t){tt_rgb[0],tt_rgb[1],tt_rgb[2]};break;
      default:break;
    }
  }
  for(unsigned i=0;i<m;++i){
    if(c[84]){if(n)bar[i]=ring[i*n/m];continue;}
    unsigned mode=c[4],index=(mode==1 || mode==5)?m-1-i:i;
    unsigned virtual_index=index*16/m;
    switch(mode){
      case 8:bar[i]=hsv(c[81],c[82],c[83]);break;
      case 1:case 2:if(virtual_index<s->level)bar[i]=hsv(-virtual_index*16,255,255);break;
      case 5:case 6:if(virtual_index==15-(s->elapsed_ms/50)%16)bar[i]=(beef_rgb_t){255,255,255};break;
      case 4:if(host_active)bar[i]=(beef_rgb_t){bar_rgb[0],bar_rgb[1],bar_rgb[2]};break;
      default:break;
    }
  }
}
