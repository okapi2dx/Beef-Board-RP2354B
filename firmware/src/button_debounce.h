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

// The first edge is immediate. The accepted phase locks out reverse edges
// for its configured window. At expiry, adopt the current raw state so a
// sustained recovery input is never lost waiting for another edge.
static inline void beef_button_debouncer_accept(beef_button_debouncer_t *state,
                                               unsigned button, uint32_t now_us) {
  uint16_t const bit = (uint16_t)(1u << button);
  state->press_guard &= (uint16_t)~bit;
  state->release_guard &= (uint16_t)~bit;
  state->started_us[button] = now_us;
  if (state->raw & bit) {
    state->accepted |= bit;
    if (state->press_window_100us[button]) state->press_guard |= bit;
  } else {
    state->accepted &= (uint16_t)~bit;
    if (state->release_window_100us[button]) state->release_guard |= bit;
  }
}

static inline void beef_button_debouncer_tick_button(beef_button_debouncer_t *state,
                                                      unsigned button,
                                                      uint32_t now_us) {
  uint16_t const bit = (uint16_t)(1u << button);
  uint16_t const window = state->accepted & bit ?
      state->press_window_100us[button] : state->release_window_100us[button];
  if (((state->press_guard | state->release_guard) & bit) &&
      (uint32_t)(now_us - state->started_us[button]) < (uint32_t)window * 100u)
    return;
  state->press_guard &= (uint16_t)~bit;
  state->release_guard &= (uint16_t)~bit;
  if ((state->raw ^ state->accepted) & bit)
    beef_button_debouncer_accept(state, button, now_us);
}

static inline void beef_button_debouncer_edge(beef_button_debouncer_t *state,
                                               unsigned button, bool pressed,
                                               uint32_t now_us,
                                               uint16_t press_window_100us,
                                               uint16_t release_window_100us) {
  uint16_t const bit = (uint16_t)(1u << button);
  state->press_window_100us[button] = press_window_100us;
  state->release_window_100us[button] = release_window_100us;
  if (pressed) state->raw |= bit;
  else state->raw &= (uint16_t)~bit;
  beef_button_debouncer_tick_button(state, button, now_us);
}

static inline void beef_button_debouncer_tick(beef_button_debouncer_t *state,
                                               uint32_t now_us) {
  for (unsigned button = 0; button < BEEF_BUTTON_COUNT; ++button)
    beef_button_debouncer_tick_button(state, button, now_us);
}
