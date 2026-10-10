"""Run AT90's production spin/ticker logic with a deterministic host clock."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
avr = root / 'at90usb1286'
manager = (avr / 'devices/iidx/iidx_rgb_manager.cpp').read_text()
start = manager.index('    void init(const config &cfg) {')
end = manager.index('\n    void update(', start)
init = manager[start:end].replace('void init(', 'void init_ring(')
start = manager.index('      bool render_rainbow(')
end = manager.index('\n      SmoothRainbow', start)
render = manager[start:end]
cases = r'''
#include <cassert>
#include <cstdio>
#include "rgb_patterns.h"
volatile uint32_t milliseconds = 0;
extern "C" void timer_arm(timer*, uint16_t) {}
extern "C" bool timer_is_expired(timer*) { return false; }
uint8_t quadwave8(uint8_t value) { return value; }
struct config {};
enum { SPIN_TIMER=50, FAST_SPIN_TIMER=25 };
namespace RgbHelper { uint8_t num_tt_leds; }
namespace Turntable { SpinPattern spin_pattern; }
struct HSV { uint8_t h,s,v; };
struct CHSV { uint8_t h,s,v; CHSV(uint8_t h,uint8_t s,uint8_t v):h(h),s(s),v(v){} };
struct CRGB {
 uint8_t r,g,b;
 CRGB():r(0),g(0),b(0){}
 CRGB(uint8_t r,uint8_t g,uint8_t b):r(r),g(g),b(b){}
 CRGB(CHSV c):r(c.h),g(c.s),b(c.v){}
 static const CRGB White;
 CRGB operator-(CRGB c) const { return CRGB(r-c.r,g-c.g,b-c.b); }
 CRGB scale8(uint8_t s) const { return CRGB(r*s/255,g*s/255,b*s/255); }
 void nscale8(uint8_t s) { *this=scale8(s); }
 void operator+=(CRGB c) { r+=c.r;g+=c.g;b+=c.b; }
 bool operator!=(CRGB c) const { return r!=c.r || g!=c.g || b!=c.b; }
};
const CRGB CRGB::White(255,255,255);
CRGB tt_leds[255];
'''
cases += init
cases += render
cases += r'''
int main() {
 for(unsigned count=1;count<=255;count++) {
  RgbHelper::num_tt_leds=count;
  Turntable::spin_pattern=SpinPattern();init_ring(config{});
  for(unsigned t=0;t<3000;t++) {
   milliseconds=t;
   Turntable::spin_pattern.update(t<1000?0:t<2000?1:-1);
   unsigned pixel=Turntable::spin_pattern.get();
   assert(pixel<count && pixel+count/2<count);
  }
  HSV color={0,180,128};render_rainbow(color,0);
  CRGB first=tt_leds[count-1];
  for(unsigned frame=0;frame<100;frame++) {
   assert(!render_rainbow(color,0));assert(!(tt_leds[count-1]!=first));
  }
  color.v=255;assert(render_rainbow(color,0));
  color.v=0;assert(render_rainbow(color,0));assert(tt_leds[count-1].b==0);
  color.v=128;assert(render_rainbow(color,0));assert(!(tt_leds[count-1]!=first));
 }
 // Full hue cycle, both directions, including the 255/0 boundary.
 for(int direction : {-1, 0, 1}) {
  milliseconds=0;SpinPattern rainbow(3,2);
  unsigned expected=direction==-1?255:1;
  rainbow.update(direction);assert(rainbow.get()==expected);
  unsigned period=direction?2:3;
  for(unsigned i=1;i<=1024;i++) {
   milliseconds=i*period;
   expected=(expected+(direction==-1?255:1))%256;
   assert(rainbow.update(direction));assert(rainbow.get()==expected);
  }
 }
 // A batch bigger than a non-power-of-two cycle must retain exact phase.
 milliseconds=0;SpinPattern bounded(1,1,7);bounded.update(-1);
 milliseconds=200;bounded.update(-1);assert(bounded.get()==2);
 for(unsigned speed=1;speed<=5;speed++) {
  milliseconds=0;SpinPattern spin(50,25,12);
  spin.update(0,speed);unsigned first=spin.get();
  unsigned duration=150/speed;
  milliseconds=duration-1;spin.update(0,speed);assert(spin.get()==first);
  milliseconds=duration;spin.update(0,speed);assert(spin.get()==(first+1)%12);
  milliseconds=0;SpinPattern active(50,25,12);
  active.update(-1,speed);first=active.get();duration=75/speed;
  milliseconds=duration;active.update(-1,speed);assert(active.get()==(first+11)%12);
 }
 puts("PASS AT90 lighting: LED counts, wrap, rainbow brightness and five spin speeds in both states");
}
'''
with tempfile.TemporaryDirectory(prefix='beef-at90-lighting-') as directory:
    temp = Path(directory)
    (temp / 'FastLED/src').mkdir(parents=True)
    (temp / 'FastLED/src/FastLED.h').write_text('#include <stdint.h>\nuint8_t quadwave8(uint8_t);\n')
    for name in ['rgb_patterns.cpp', 'rgb_patterns.h', 'ticker.cpp', 'ticker.h', 'timer.h']:
        (temp / name).write_bytes((avr / name).read_bytes())
    (temp / 'test.cpp').write_text(cases)
    executable = temp / ('test.exe' if os.name == 'nt' else 'test')
    subprocess.run([os.environ.get('CXX','g++'), '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-include', 'cstdint', '-include', 'initializer_list', '-include', str(temp/'timer.h'), '-I'+str(temp),
                    str(temp/'test.cpp'), str(temp/'rgb_patterns.cpp'), str(temp/'ticker.cpp'),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
