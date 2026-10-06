#pragma once
#include <stdint.h>
#include "timer.h"
#include "../src/button_debounce.h"

// Use the same production state machine as RP, with AVR's 100us clock.
template<int BUTTONS>
class Debouncer {
public:
  void init(uint8_t new_window_ms) {
    beef_button_debouncer_init(&state, 0);
    for (uint8_t bit = 0; bit < BUTTONS; ++bit)
      state.press_window_100us[bit] = state.release_window_100us[bit] = uint16_t(new_window_ms) * 10u;
  }
  void init(const uint8_t *press_windows, const uint8_t *release_windows,
            uint8_t count, uint8_t default_window_ms = 0) {
    beef_button_debouncer_init(&state, 0);
    for (uint8_t bit = 0; bit < BUTTONS; ++bit) {
      state.press_window_100us[bit] = bit < count ? press_windows[bit] : uint16_t(default_window_ms) * 10u;
      state.release_window_100us[bit] = bit < count ? release_windows[bit] : uint16_t(default_window_ms) * 10u;
    }
  }
  uint16_t debounce(uint16_t buttons, uint16_t mask) {
    return (buttons & ~mask) | debounce(buttons & mask);
  }
  void configure(uint8_t window_ms) {
    for (uint8_t bit = 0; bit < BUTTONS; ++bit)
      configure_button(bit, uint16_t(window_ms) * 10u, uint16_t(window_ms) * 10u);
  }
  void configure(const uint8_t *press_windows, const uint8_t *release_windows,
                 uint8_t count, uint8_t default_window_ms = 0) {
    for (uint8_t bit = 0; bit < BUTTONS; ++bit)
      configure_button(bit,
        bit < count ? press_windows[bit] : uint16_t(default_window_ms) * 10u,
        bit < count ? release_windows[bit] : uint16_t(default_window_ms) * 10u);
  }
  uint16_t debounce(uint16_t buttons) {
    uint32_t const now = button_time_100us_now() * 100u;
    for (uint8_t bit = 0; bit < BUTTONS; ++bit)
      beef_button_debouncer_edge(&state, bit, (buttons & (uint16_t(1) << bit)) != 0,
        now, state.press_window_100us[bit], state.release_window_100us[bit]);
    return state.accepted;
  }
private:
  void configure_button(uint8_t button, uint16_t press, uint16_t release) {
    if (state.press_window_100us[button] == press &&
        state.release_window_100us[button] == release) return;
    // Match RP live edits: adopt the raw state only for a changed button.
    // An unrelated config save must preserve active suppression windows.
    const uint16_t bit = uint16_t(1) << button;
    state.press_window_100us[button] = press;
    state.release_window_100us[button] = release;
    state.press_guard &= uint16_t(~bit);
    state.release_guard &= uint16_t(~bit);
    state.accepted = (state.accepted & uint16_t(~bit)) | (state.raw & bit);
  }
  static_assert(BUTTONS <= BEEF_BUTTON_COUNT, "Too many debounce buttons");
  beef_button_debouncer_t state{};
};
