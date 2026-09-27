#include <stdint.h>
#include <cassert>
uint32_t milliseconds;
#include "../debounce.h"
int main() {
  Debouncer<11> d;
  d.init(50);
  milliseconds=0; assert(d.debounce(1)==1);
  milliseconds=1; assert(d.debounce(0)==0);
  milliseconds=10; assert(d.debounce(1)==0);
  milliseconds=50; assert(d.debounce(1)==0);
  assert(d.debounce(0)==0); assert(d.debounce(1)==1);
  milliseconds=51; assert(d.debounce(3)==3);
  milliseconds=200; assert(d.debounce(3)==3);
  assert(d.debounce(0)==0);
  d.init(50); milliseconds=0xfffffff0u; assert(d.debounce(1)==1);
  assert(d.debounce(0)==0);
  milliseconds=10; assert(d.debounce(1)==0); assert(d.debounce(0)==0);
  milliseconds=34; assert(d.debounce(1)==1);
  d.init(0); assert(d.debounce(1)==1); assert(d.debounce(0)==0); assert(d.debounce(1)==1);
  d.init(50); assert(d.debounce(0x8001,0x7ff)==0x8001);
}
