"""Exercise the production RGB snapshot and diagnostic scheduler on the host."""
from pathlib import Path
import os
import subprocess
import tempfile

avr = Path(__file__).resolve().parents[1] / 'at90usb1286'
source = (avr / 'beef.cpp').read_text()
start = source.index('static void snapshot_turntable_rgb()')
end = source.index('\n// bit-field', start)
snapshot = source[start:end]
start = source.index('        static uint32_t last_counts_ms = 0;')
end = source.index('\n      }\n      return usb_handler', start)
scheduler = source[start:end]
test = r'''
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include "diagnostics.h"
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(x) if (true)
enum { HID_REPORTID_ChatterCounts=7, HID_REPORTID_DiagnosticsStream=5 };
struct Color { uint8_t r,g,b; } pixel{}, *tt_leds=&pixel;
struct { bool disable_leds=false; } current_config;
namespace RgbHelper { uint8_t num_tt_leds=24; }
uint32_t milliseconds=0;
volatile diagnostics_report diagnostics{};
volatile chatter_counts_report chatter_counts{};
'''
test += snapshot
test += '\nbool stream(uint8_t* ReportID, void* ReportData, uint16_t* ReportSize) {\n' + scheduler + '\n}\n'
test += r'''
int main() {
  static_assert(offsetof(diagnostics_report,tt_rgb)==60,"Preserve all old offsets");
  diagnostics_report packet{};
  uint8_t id=0, previous[3]={}; uint16_t size=0;
  unsigned samples=0, counts=0;
  for(milliseconds=0;milliseconds<=1000;++milliseconds) {
    pixel={uint8_t(milliseconds),uint8_t(milliseconds+1),uint8_t(milliseconds+2)};
    const bool forced=stream(&id,&packet,&size);
    if(id==7) { assert(size==28);++counts;continue; }
    assert(id==5 && size==63);
    if(forced) {
      ++samples;
      assert(packet.tt_rgb[0]==pixel.r && packet.tt_rgb[1]==pixel.g && packet.tt_rgb[2]==pixel.b);
      memcpy(previous,packet.tt_rgb,3);
    } else assert(memcmp(previous,packet.tt_rgb,3)==0);
  }
  assert(counts==50 && samples==49);
  current_config.disable_leds=true; snapshot_turntable_rgb();
  assert(diagnostics.tt_rgb[0]==0 && diagnostics.tt_rgb[1]==0 && diagnostics.tt_rgb[2]==0);
  current_config.disable_leds=false; tt_leds=nullptr; snapshot_turntable_rgb();
  assert(diagnostics.tt_rgb[0]==0);
  puts("PASS AT90 RGB: 64-byte packet, stable old offsets, scheduled-only snapshots, disabled/uninitialized LEDs");
}
'''
with tempfile.TemporaryDirectory(prefix='beef-rgb-diagnostics-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(test)
    exe = path / ('test.exe' if os.name == 'nt' else 'test')
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(avr), str(path / 'test.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
