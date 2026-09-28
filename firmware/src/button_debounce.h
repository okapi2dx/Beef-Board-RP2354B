#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BEEF_BUTTON_COUNT 11u

typedef struct {
  uint16_t raw;
  uint16_t accepted;
  uint16_t cooling;
  uint16_t pending_release;
  uint32_t started_us[BEEF_BUTTON_COUNT];
} beef_button_debouncer_t;

static inline void beef_button_debouncer_init(beef_button_debouncer_t *state,
                                               uint16_t initial) {
  state->raw = initial;
  state->accepted = initial;
  state->cooling = 0;
  state->pending_release = 0;
  for (unsigned i = 0; i < BEEF_BUTTON_COUNT; ++i) state->started_us[i] = 0;
}

// A press is immediate. A release during the bounce window is held until the
// window ends, so a short contact bounce cannot discard a continuing hold.
static inline void beef_button_debouncer_edge(beef_button_debouncer_t *state,
                                               unsigned button, bool pressed,
                                               uint32_t now_us, uint8_t window_ms) {
  uint16_t const bit = (uint16_t)(1u << button);
  bool const previous = (state->raw & bit) != 0;
  if (previous == pressed) return;

  if (pressed) state->raw |= bit;
  else state->raw &= (uint16_t)~bit;

  if (!window_ms) {
    if (pressed) state->accepted |= bit;
    else state->accepted &= (uint16_t)~bit;
    state->cooling &= (uint16_t)~bit;
    state->pending_release &= (uint16_t)~bit;
    return;
  }

  bool const within_window = (state->cooling & bit) &&
      (uint32_t)(now_us - state->started_us[button]) <
          (uint32_t)window_ms * 1000u;
  if (pressed) {
    state->pending_release &= (uint16_t)~bit;
    if (state->accepted & bit) return;
    state->accepted |= bit;
    state->cooling |= bit;
    state->started_us[button] = now_us;
  } else if (within_window) {
    state->pending_release |= bit;
  } else {
    state->accepted &= (uint16_t)~bit;
    state->cooling &= (uint16_t)~bit;
    state->pending_release &= (uint16_t)~bit;
  }
}

static inline void beef_button_debouncer_tick(beef_button_debouncer_t *state,
                                               uint32_t now_us,
                                               uint8_t window_ms) {
  for (unsigned button = 0; button < BEEF_BUTTON_COUNT; ++button) {
    uint16_t const bit = (uint16_t)(1u << button);
    if ((state->pending_release & bit) && !(state->raw & bit) &&
        (uint32_t)(now_us - state->started_us[button]) >=
            (uint32_t)window_ms * 1000u) {
      state->accepted &= (uint16_t)~bit;
      state->cooling &= (uint16_t)~bit;
      state->pending_release &= (uint16_t)~bit;
    }
  }
}
