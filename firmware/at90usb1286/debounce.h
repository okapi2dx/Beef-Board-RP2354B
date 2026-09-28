#pragma once
#include <stdint.h>
#include <string.h>
#ifdef __AVR__
#include <util/atomic.h>
#endif

template<int BUTTONS>
class Debouncer {
public:
  void init(uint8_t new_window) {
    window = new_window;
    accepted = cooling = 0;
    memset(started, 0, sizeof(started));
  }
  uint16_t debounce(uint16_t buttons, uint16_t mask) {
    return (buttons & ~mask) | debounce(buttons & mask);
  }
  uint16_t debounce(uint16_t buttons) {
    uint32_t now;
#ifdef __AVR__
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { now = milliseconds; }
#else
    now = milliseconds;
#endif
    if (!window) { accepted = buttons; cooling = 0; return buttons; }
    // Press immediately. Hold a release during the bounce window so a
    // release/re-press bounce cannot lose a continuing physical hold.
    for (uint8_t bit = 0; bit < BUTTONS; ++bit) {
      const uint16_t flag = uint16_t(1) << bit;
      if ((cooling & flag) && uint32_t(now - started[bit]) >= window)
        cooling &= ~flag;
      if (buttons & flag) {
        if (!(accepted & flag)) {
          accepted |= flag;
          cooling |= flag;
          started[bit] = now;
        }
      } else if (!(cooling & flag)) {
        accepted &= ~flag;
      }
    }
    return accepted;
  }
private:
  uint32_t started[BUTTONS]{};
  uint16_t accepted{}, cooling{};
  uint8_t window{};
};
