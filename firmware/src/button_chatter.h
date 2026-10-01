#pragma once

#include <stdint.h>

// Diagnostic only: group physical edges for up to the configurable 16 ms
// range. The event bit records an opposite-edge bounce candidate so automatic
// tuning can still work from a 0.0 ms setting. close_edges only counts a
// candidate that the currently configured debounce window would suppress.
// This never changes controller input state.
#define BEEF_CHATTER_BUTTONS 11u
#define BEEF_CHATTER_EVENT_COUNT 6u
#define BEEF_CHATTER_OBSERVATION_US 16000u

typedef struct {
  uint16_t raw;
  uint16_t seen;
  uint16_t release_phase;
  uint32_t phase_start_us[BEEF_CHATTER_BUTTONS];
  uint32_t last_edge_us[BEEF_CHATTER_BUTTONS];
  uint16_t edge_count[7];
  uint16_t close_edges[BEEF_CHATTER_BUTTONS];
  uint16_t event_sequence;
  // Each ring entry is [button/state, elapsed 100us low, elapsed high].
  // Low 14 bits: interval (10000 means >999.9 ms). Bit 14 marks a
  // firmware decision; bit 15 is that decision. No report size change.
  uint8_t events[BEEF_CHATTER_EVENT_COUNT * 3u];
} beef_button_chatter_t;

static inline void beef_button_chatter_init(beef_button_chatter_t *state,
                                            uint16_t raw) {
  state->raw = raw;
  state->seen = 0;
  state->release_phase = 0;
  for (unsigned i = 0; i < BEEF_CHATTER_BUTTONS; ++i) {
    state->phase_start_us[i] = 0;
    state->last_edge_us[i] = 0;
    state->close_edges[i] = 0;
  }
  for (unsigned i = 0; i < 7u; ++i) state->edge_count[i] = 0;
  state->event_sequence = 0;
  for (unsigned i = 0; i < BEEF_CHATTER_EVENT_COUNT * 3u; ++i) state->events[i] = 0xffu;
}

static inline void beef_button_chatter_edge(beef_button_chatter_t *state,
                                            unsigned button, int pressed,
                                            uint32_t now_us,
                                            uint16_t press_threshold_100us,
                                            uint16_t release_threshold_100us) {
  uint16_t const bit = (uint16_t)(1u << button);
  if (((state->raw & bit) != 0) == (pressed != 0)) return;
  ++state->event_sequence;
  if (button < 7u) ++state->edge_count[button];
  uint32_t const elapsed_us = now_us - state->last_edge_us[button];
  int release_phase = (state->release_phase & bit) != 0;
  int const same_phase = (state->seen & bit) &&
      (uint32_t)(now_us - state->phase_start_us[button]) < BEEF_CHATTER_OBSERVATION_US;
  if (!same_phase) {
    release_phase = !pressed;
    state->phase_start_us[button] = now_us;
    if (release_phase) state->release_phase |= bit;
    else state->release_phase &= (uint16_t)~bit;
  }
  int const candidate = same_phase && (release_phase ? pressed : !pressed);
  uint16_t const threshold_100us =
      release_phase ? release_threshold_100us : press_threshold_100us;
  int const detected = candidate && threshold_100us &&
      (uint32_t)(now_us - state->phase_start_us[button]) <
          (uint32_t)threshold_100us * 100u;
  if (detected) ++state->close_edges[button];
  unsigned const slot = (state->event_sequence % BEEF_CHATTER_EVENT_COUNT) * 3u;
  uint32_t const units = elapsed_us / 100u;
  uint16_t const interval = units > 9999u ? 10000u : (uint16_t)units;
  state->events[slot] = (uint8_t)((pressed ? 0x80u : 0u) |
                                  (candidate ? 0x40u : 0u) |
                                  (release_phase ? 0x20u : 0u) |
                                  (!same_phase ? 0x10u : 0u) | button);
  uint16_t const event_interval = interval | 0x4000u | (detected ? 0x8000u : 0u);
  state->events[slot + 1u] = (uint8_t)event_interval;
  state->events[slot + 2u] = (uint8_t)(event_interval >> 8);
  state->seen |= bit;
  state->last_edge_us[button] = now_us;
  if (pressed) state->raw |= bit;
  else state->raw &= (uint16_t)~bit;
}

static inline void beef_button_chatter_sample(beef_button_chatter_t *state,
                                              uint16_t raw, uint32_t now_us,
                                              const uint16_t *press_threshold_100us,
                                              const uint16_t *release_threshold_100us) {
  uint16_t changed = (uint16_t)(state->raw ^ raw);
  for (unsigned i = 0; i < BEEF_CHATTER_BUTTONS; ++i)
    if (changed & (1u << i))
      beef_button_chatter_edge(state, i, raw & (1u << i), now_us,
          press_threshold_100us[i], release_threshold_100us[i]);
}
