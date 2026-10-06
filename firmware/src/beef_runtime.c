#include "beef_runtime.h"
#include <string.h>
#include "turntable_sensitivity.h"

bool beef_config_valid(const uint8_t *c) {
  if ((c[0] != 33 && c[0] != 34 && c[0] != 35 && c[0] != 36 && c[0] != 37 && c[0] != 38 && c[0] != 39 && c[0] != 40 && c[0] != 41 && c[0] != 42 && c[0] != 43 && c[0] != 44 && c[0] != 45) || c[2] > 9 || (c[3] > (c[0] >= 43 ? 50 : 30)) || c[4] > 8 ||
      c[30] < 1 || c[30] > 10 || c[31] > 2 || c[32] > 1 || c[33] > 1 ||
      c[78] < 1 || c[78] > 60 || c[79] < 1 ||
      c[88] > 100 || c[124] > 2 || ((uint16_t)c[125] + 256u*c[126]) > 1000) return false;
  if (c[0] >= 45 && ((uint16_t)c[71] | ((uint16_t)c[72] << 8)) > 2000) return false;
  if (c[0] >= 42 && c[61] != 0 && (c[61] < 2 || c[61] > 16 || (c[61] & 1u))) return false;
  if (c[0] >= 34)
    for (unsigned i = 0; i < 7; ++i) if (c[48+i] > (c[0] >= 37 ? 160 : 50)) return false;
  if (c[0] >= 36)
    for (unsigned i = 0; i < 7; ++i) if (c[64+i] > (c[0] >= 37 ? 160 : 50)) return false;
  for (unsigned i = 0; i < 11; ++i)
    if (c[102+i] > 100 || c[127+i] > 11) return false;
  return true;
}

void beef_keyboard_report(const uint8_t *cfg, uint16_t buttons, uint8_t keys[13]) {
  memset(keys, 0, 13);
  unsigned used = 0;
  for (unsigned i = 0; i < 13; ++i) {
    if (!(buttons & (1u << i))) continue;
    uint8_t key = cfg[35+i];
    bool duplicate = false;
    for (unsigned j = 0; j < used; ++j) duplicate |= keys[j] == key;
    if (key && !duplicate) keys[used++] = key;
  }
}

// Gate new gestures and deliberate reversals; ignore small jitter without
// repeatedly stopping accepted same-direction motion.
void beef_turntable_update(beef_turntable_t *s, const uint8_t *c,
                           int32_t transitions, uint32_t now) {
  uint8_t settings[8] = {c[1], c[3], c[30], c[34], c[85], c[86], c[71], c[72]};
  if (!s->initialized || memcmp(settings, s->settings, sizeof(settings))) {
    uint8_t position = s->initialized ? s->output.position : 127;
    memset(s, 0, sizeof(*s));
    s->raw = s->filtered = s->position = position;
    s->previous.position = s->output.position = position;
    for (unsigned i=0; i<256; ++i) s->history[i] = s->previous;
    memcpy(s->settings, settings, sizeof(settings));
    s->last_ms = s->gate.last_motion = s->motion_ms = now;
    s->initialized = true;
  }
  uint32_t elapsed = now - s->last_ms;
  int32_t signed_steps = c[1] ? -transitions : transitions;
  int32_t denominator = c[0] >= 45 ? 1000u : 12u;
  unsigned sensitivity = c[0] >= 45 ? (uint16_t)c[71] | ((uint16_t)c[72] << 8) : beef_sensitivity_units(c[30]);
  s->fraction += signed_steps * (int32_t)sensitivity;
  int32_t delta = s->fraction / denominator;
  s->fraction %= denominator;
  s->raw = (uint8_t)(s->raw + delta);
  const int32_t qualified = beef_encoder_gate_update(&s->gate, signed_steps, now, c[3]);
  const int32_t accepted_steps = sensitivity ? qualified : 0;
  s->accepted_fraction += accepted_steps * (int32_t)sensitivity;
  const int32_t accepted_delta = s->accepted_fraction / denominator;
  s->accepted_fraction %= denominator;
  const bool accepted = accepted_delta != 0;
  if (accepted) {
    s->filtered = (uint8_t)(s->filtered + accepted_delta);
    s->position = (uint8_t)(s->position + accepted_delta);
    s->direction = accepted_delta > 0 ? 1 : -1; s->moving = true;
    s->motion_ms = now; s->hold_remainder = 0;
  } else if (beef_encoder_gate_reversing(&s->gate)) {
    // Do not synthesize old-direction X motion while evaluating a reversal.
    s->direction = 0; s->moving = false; s->hold_remainder = 0;
  }
  // Both report modes use only encoder-qualified motion; rejected movement
  // must not start or extend a digital turntable press.
  if (accepted_steps) {
    s->digital_direction = accepted_steps > 0 ? 1 : -1;
    s->digital_motion_ms = now;
  } else if ((uint32_t)(now-s->digital_motion_ms) >= c[34]) s->digital_direction = 0;
  if (!accepted && s->moving) {
    uint32_t age = now - s->motion_ms;
    uint32_t previous_age = s->last_ms - s->motion_ms;
    uint32_t hold_now = age < c[34] ? age : c[34];
    uint32_t hold_prev = previous_age < c[34] ? previous_age : c[34];
    if (hold_now > hold_prev) {
      // Fixed 100 counts/s, bounded by time since the last real accepted motion.
      uint32_t numerator = s->hold_remainder + (hold_now - hold_prev) * 100;
      s->position = (uint8_t)(s->position + s->direction * (int32_t)(numerator / 1000));
      s->hold_remainder = numerator % 1000;
    }
    if (age >= c[34]) { s->direction = 0; s->moving = false; }
  }
  beef_tt_sample_t sample = {s->position, s->digital_direction};
  // Delay samples, including releases. Unlike an event queue this cannot
  // overflow at high encoder rates or reorder events when delay is edited.
  if (elapsed >= 256) {
    for (unsigned i=0; i<256; ++i) s->history[i] = s->previous;
  } else {
    for (uint32_t i=1; i<=elapsed; ++i) s->history[(uint8_t)(s->last_ms+i)] = s->previous;
  }
  s->history[(uint8_t)now] = sample;
  s->output = c[86] ? s->history[(uint8_t)(now-c[86])] : sample;
  s->previous = sample; s->last_ms = now;
}
