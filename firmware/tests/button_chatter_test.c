#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "button_chatter.h"

static uint8_t last_code(const beef_button_chatter_t *state) {
  return state->events[(state->event_sequence % BEEF_CHATTER_EVENT_COUNT) * 3u];
}

int main(void) {
  beef_button_chatter_t state;
  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, 1000, 0, 0);
  assert(state.close_edges[0] == 0 && state.edge_count[0] == 1);
  assert((last_code(&state) & 0x10u) == 0x10u);
  beef_button_chatter_edge(&state, 0, 0, 1400, 0, 0);
  assert(state.close_edges[0] == 0 && (last_code(&state) & 0x60u) == 0x40u);
  assert((last_code(&state) & 0x10u) == 0u);
  beef_button_chatter_edge(&state, 0, 1, 1900, 0, 0);
  assert(state.close_edges[0] == 0 && (last_code(&state) & 0x60u) == 0u);

  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, 1000, 5, 5);
  beef_button_chatter_edge(&state, 0, 0, 1400, 5, 5);
  assert(state.close_edges[0] == 1); // 0.4 ms is suppressed by a 0.5 ms window.
  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, 1000, 4, 4);
  beef_button_chatter_edge(&state, 0, 0, 1400, 4, 4);
  assert(state.close_edges[0] == 0); // The exact guard boundary is not suppressed.

  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, 1000, 5, 5);
  beef_button_chatter_edge(&state, 0, 0, 20000, 0, 0);
  assert(state.close_edges[0] == 0 && (last_code(&state) & 0x60u) == 0x20u);
  assert((last_code(&state) & 0x10u) == 0x10u);
  beef_button_chatter_edge(&state, 0, 1, 20400, 0, 5);
  assert(state.close_edges[0] == 1 && (last_code(&state) & 0x60u) == 0x60u);
  beef_button_chatter_edge(&state, 0, 0, 20900, 0, 0);
  assert(state.close_edges[0] == 1 && (last_code(&state) & 0x60u) == 0x20u);
  beef_button_chatter_edge(&state, 0, 1, 21200, 0, 0);
  assert(state.close_edges[0] == 1); // Candidate remains observable even when setting is zero.

  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, UINT32_MAX - 200u, 0, 0);
  beef_button_chatter_edge(&state, 0, 0, 100u, 0, 0);
  assert((last_code(&state) & 0x40u) != 0); // Candidate observation survives clock wrap.
  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, 1000, 0, 0);
  beef_button_chatter_edge(&state, 0, 0, 17000, 0, 0);
  assert(state.close_edges[0] == 0); // Exactly 16 ms starts release.
  beef_button_chatter_init(&state, 0);
  beef_button_chatter_edge(&state, 0, 1, 1000, 8, 8);
  beef_button_chatter_edge(&state, 0, 0, 1299, 8, 8);
  beef_button_chatter_edge(&state, 0, 1, 1598, 8, 8);
  beef_button_chatter_edge(&state, 0, 0, 1897, 8, 8);
  // Rounded intervals sum to 0.6 ms, but real phase time is 0.897 ms.
  // Carry the firmware decision so the app cannot call this suppressed.
  unsigned slot = (state.event_sequence % BEEF_CHATTER_EVENT_COUNT) * 3u;
  uint16_t word = state.events[slot + 1u] | ((uint16_t)state.events[slot + 2u] << 8);
  assert(state.close_edges[0] == 1);
  assert((word & 0xc000u) == 0x4000u && (word & 0x3fffu) == 2u);
  puts("PASS button chatter: candidates, active thresholds, phase attribution, zero setting, wrap");
  return 0;
}
