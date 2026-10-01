#pragma once
#include <stdint.h>
#include <string.h>
#include "timer.h"
#ifdef __AVR__
#include <util/atomic.h>
#endif

template<int BUTTONS>
class Debouncer {
public:
  void init(uint8_t new_window_ms) {
    for (uint8_t bit = 0; bit < BUTTONS; ++bit)
      press_window[bit] = release_window[bit] = uint16_t(new_window_ms) * 10u;
    reset_state();
  }
  void init(const uint8_t *press_windows, const uint8_t *release_windows,
            uint8_t count, uint8_t default_window_ms = 0) {
    for (uint8_t bit = 0; bit < BUTTONS; ++bit) {
      press_window[bit] = bit < count ? press_windows[bit] : uint16_t(default_window_ms) * 10u;
      release_window[bit] = bit < count ? release_windows[bit] : uint16_t(default_window_ms) * 10u;
    }
    reset_state();
  }
  uint16_t debounce(uint16_t buttons, uint16_t mask) {
    return (buttons & ~mask) | debounce(buttons & mask);
  }
  uint16_t debounce(uint16_t buttons) {
    uint32_t now = button_time_100us_now();
    for (uint8_t bit = 0; bit < BUTTONS; ++bit) {
      const uint16_t flag = uint16_t(1) << bit;
      const bool pressed = (buttons & flag) != 0;
      if ((press_guard & flag) && uint32_t(now - started[bit]) >= press_window[bit]) {
        press_guard &= ~flag;
        if (!(raw & flag)) {
          accepted &= ~flag;
          if (release_window[bit]) release_guard |= flag;
          started[bit] = now;
        }
      } else if ((release_guard & flag) && uint32_t(now - started[bit]) >= release_window[bit]) {
        release_guard &= ~flag;
        // A rejected ON pulse remains rejected until the next fresh ON edge.
      }
      if (((raw & flag) != 0) == pressed) continue;
      if (pressed) raw |= flag;
      else raw &= ~flag;
      if (((accepted & flag) != 0) == pressed) continue;
      if (pressed) {
        if ((release_guard & flag) && uint32_t(now - started[bit]) < release_window[bit]) continue;
        accepted |= flag;
        release_guard &= ~flag;
        if (press_window[bit]) press_guard |= flag;
        else press_guard &= ~flag;
      } else {
        if ((press_guard & flag) && uint32_t(now - started[bit]) < press_window[bit]) continue;
        accepted &= ~flag;
        press_guard &= ~flag;
        if (release_window[bit]) release_guard |= flag;
        else release_guard &= ~flag;
      }
      started[bit] = now;
    }
    return accepted;
  }
private:
  void reset_state() {
    memset(started, 0, sizeof(started));
    accepted = raw = press_guard = release_guard = 0;
  }
  uint32_t started[BUTTONS]{};
  uint16_t accepted{}, raw{}, press_guard{}, release_guard{};
  uint16_t press_window[BUTTONS]{};
  uint16_t release_window[BUTTONS]{};
};
