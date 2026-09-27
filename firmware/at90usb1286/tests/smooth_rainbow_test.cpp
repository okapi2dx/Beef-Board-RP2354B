#include "../smooth_rainbow.h"
#include <cassert>
#include <cstdio>
uint16_t step(uint8_t a,uint8_t b){SmoothRainbow s;s.update(0,a,false,3);return s.update(50,b,false,3);}
int main(){
 assert(step(255,0)==step(100,101)); assert(step(0,255)==step(100,99));
 assert(step(100,101)!=step(100,108));
 SmoothRainbow s;s.update(0,100,false,3);auto a=s.update(50,101,false,3);auto b=s.update(66,101,false,3);assert(a!=b);
 for(unsigned t=82;t<230;t+=16)s.update(t,101,false,3);
 a=s.update(250,101,false,3);assert(a==s.update(300,101,false,3));
 s.update(350,110,false,3);a=s.update(366,110,false,3);b=s.update(400,109,false,3);assert(static_cast<int16_t>(b-a)>0);
 puts("PASS: smoothing, 150ms stop, reversal, both axis wraps and speed response");
}
