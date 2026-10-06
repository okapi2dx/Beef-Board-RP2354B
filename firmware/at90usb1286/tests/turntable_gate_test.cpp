#include "../turntable_gate.h"
#include <assert.h>
#include <stdio.h>
int main() {
  for (unsigned angle=0; angle<=255; ++angle) {
    const unsigned threshold=angle+1;
    for (unsigned spacing=1; spacing<=250; ++spacing) {
      for (int sign=-1; sign<=1; sign+=2) {
        TurntableGate gate; gate.reset(127,127,0);
        uint8_t raw=127, expected=127;
        for (unsigned edge=1; edge<=257; ++edge) {
          raw=static_cast<uint8_t>(raw+sign);
          if (!threshold || edge>=threshold) expected=static_cast<uint8_t>(expected+sign);
          assert(gate.update(raw,edge*spacing,angle)==expected);
        }
        for (unsigned edge=1; edge<=257; ++edge) {
          raw=static_cast<uint8_t>(raw-sign);
          if (!threshold || edge>=threshold) expected=static_cast<uint8_t>(expected-sign);
          assert(gate.update(raw,(257+edge)*spacing,angle)==expected);
        }
        assert(gate.update(raw,514*spacing+2000,angle)==expected);
        raw=static_cast<uint8_t>(raw-sign);
        if(threshold<=1)expected=static_cast<uint8_t>(expected-sign);
        assert(gate.update(raw,514*spacing+2001,angle)==expected);
      }
    }
  }
  TurntableGate gate;gate.reset(127,127,0xfffffff0UL);
  for(unsigned edge=1;edge<=22;edge++)assert(gate.update(127+edge,0xfffffff0UL+edge,21)==(edge<22?127:128));
  puts("PASS AT90 encoder gate: 0..255 counts, slow/fast, reversal, restart, wrap");
}
