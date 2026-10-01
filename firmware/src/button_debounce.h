#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BEEF_BUTTON_COUNT 11u

typedef struct {
  uint16_t raw;
  uint16_t accepted;
  uint16_t press_guard;
  uint16_t release_guard;
  uint32_t started_us[BEEF_BUTTON_COUNT];
  uint16_t press_window_100us[BEEF_BUTTON_COUNT];
  uint16_t release_window_100us[BEEF_BUTTON_COUNT];
} beef_button_debouncer_t;

static inline void beef_button_debouncer_init(beef_button_debouncer_t *state,
                                               uint16_t initial) {
  state->raw = initial;
  state->accepted = initial;
  state->press_guard = 0;
  state->release_guard = 0;
  for (unsigned i = 0; i < BEEF_BUTTON_COUNT; ++i) {
    state->started_us[i] = 0;
    state->press_window_100us[i] = 0;
    state->release_window_100us[i] = 0;
  }
}

// Both intended edges are immediate. Ignore opposite edges only within the
// guard started by the last accepted operation; ignored edges do not restart it.
static inline void beef_button_debouncer_edge(beef_button_debouncer_t *state,
                                               unsigned button, bool pressed,
                                               uint32_t now_us,
                                               uint16_t press_window_100us,
                                               uint16_t release_window_100us) {
  uint16_t const bit = (uint16_t)(1u << button);
  if (((state->raw & bit) != 0) == pressed) return;
  state->press_window_100us[button] = press_window_100us;
  state->release_window_100us[button] = release_window_100us;

  if (pressed) state->raw |= bit;
  else state->raw &= (uint16_t)~bit;
  if (((state->accepted & bit) != 0) == pressed) return;

  if (pressed) {
    if ((state->release_guard & bit) && release_window_100us &&
        (uint32_t)(now_us - state->started_us[button]) <
            (uint32_t)release_window_100us * 100u) return;
    state->accepted |= bit;
    state->release_guard &= (uint16_t)~bit;
    if (press_window_100us) state->press_guard |= bit;
    else state->press_guard &= (uint16_t)~bit;
  } else {
    if ((state->press_guard & bit) && press_window_100us &&
        (uint32_t)(now_us - state->started_us[button]) <
            (uint32_t)press_window_100us * 100u) return;
    state->accepted &= (uint16_t)~bit;
    state->press_guard &= (uint16_t)~bit;
    if (release_window_100us) state->release_guard |= bit;
    else state->release_guard &= (uint16_t)~bit;
  }
  state->started_us[button] = now_us;
}

static inline void beef_button_debouncer_tick_button(beef_button_debouncer_t *state,
                                                      unsigned button,
                                                      uint32_t now_us) {
  uint16_t const bit = (uint16_t)(1u << button);
  if ((state->press_guard & bit) &&
      (uint32_t)(now_us - state->started_us[button]) >=
          (uint32_t)state->press_window_100us[button] * 100u) {
    state->press_guard &= (uint16_t)~bit;
    if (!(state->raw & bit)) {
      state->accepted &= (uint16_t)~bit;
      if (state->release_window_100us[button]) state->release_guard |= bit;
      state->started_us[button] = now_us;
    }
  } else if ((state->release_guard & bit) &&
      (uint32_t)(now_us - state->started_us[button]) >=
          (uint32_t)state->release_window_100us[button] * 100u) {
    state->release_guard &= (uint16_t)~bit;
    // An ON edge rejected during this guard stays rejected even if its pulse
    // outlasts the guard. Only a later fresh ON edge can start another press.
  }
}

static inline void beef_button_debouncer_tick(beef_button_debouncer_t *state,
                                               uint32_t now_us) {
  for (unsigned button = 0; button < BEEF_BUTTON_COUNT; ++button)
    beef_button_debouncer_tick_button(state, button, now_us);
}
