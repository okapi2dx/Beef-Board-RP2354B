#include "beef_runtime.h"
#include <string.h>

bool beef_config_valid(const uint8_t *c) {
  if ((c[0] != 33 && c[0] != 34 && c[0] != 35 && c[0] != 36 && c[0] != 37 && c[0] != 38 && c[0] != 39 && c[0] != 40 && c[0] != 41) || c[2] > 9 || c[3] > 30 || c[4] > 8 ||
      c[30] < 1 || c[30] > 10 || c[31] > 2 || c[32] > 1 || c[33] > 1 ||
      c[78] < 1 || c[78] > 60 || c[79] < 1 || c[80] < 1 || c[138] < 1 ||
      c[88] > 100 || c[124] > 2 || ((uint16_t)c[125] + 256u*c[126]) > 1000) return false;
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

// The 8-bit axis describes 360 degrees. Gate each new gesture and reversal;
// movement before the threshold is discarded, as in the original controller.
void beef_turntable_update(beef_turntable_t *s, const uint8_t *c,
                           int32_t transitions, uint32_t now) {
  uint8_t settings[6] = {c[1], c[3], c[30], c[34], c[85], c[86]};
  if (!s->initialized || memcmp(settings, s->settings, sizeof(settings))) {
    uint8_t position = s->initialized ? s->output.position : 127;
    memset(s, 0, sizeof(*s));
    s->raw = s->filtered = s->position = position;
    s->previous.position = s->output.position = position;
    for (unsigned i=0; i<256; ++i) s->history[i] = s->previous;
    memcpy(s->settings, settings, sizeof(settings));
    s->last_ms = s->gate_ms = s->motion_ms = now;
    s->initialized = true;
  }
  uint32_t elapsed = now - s->last_ms;
  int32_t signed_steps = c[1] ? -transitions : transitions;
  unsigned sensitivity = c[30] < 1 ? 1 : c[30] > 10 ? 10 : c[30];
  s->fraction += signed_steps * (int32_t)sensitivity;
  int32_t delta = s->fraction / 10;
  s->fraction %= 10;
  s->raw = (uint8_t)(s->raw + delta);
  // The angle gate is for the analog axis. Digital TT follows raw movement,
  // including gestures smaller than the analog deadzone.
  if (delta) {
    s->digital_direction = delta > 0 ? 1 : -1;
    s->digital_motion_ms = now;
  } else if ((uint32_t)(now-s->digital_motion_ms) >= c[34]) s->digital_direction = 0;
  bool accepted = false;
  if (delta) {
    int8_t direction = delta > 0 ? 1 : -1;
    uint32_t gap = now - s->gate_ms;
    uint32_t continuity = s->gate_interval ? s->gate_interval * 3 : 250;
    if (continuity < 30) continuity = 30;
    if (continuity > 1000) continuity = 1000;
    if (direction != s->gate_direction || gap > continuity) {
      s->accumulated = 0; s->qualified = false; s->gate_interval = 0;
    } else if (gap) s->gate_interval = gap;
    s->gate_direction = direction; s->gate_ms = now;
    unsigned magnitude = delta < 0 ? -delta : delta;
    unsigned accumulated = s->accumulated + magnitude;
    s->accumulated = accumulated > 65535 ? 65535 : accumulated;
    unsigned threshold = ((unsigned)c[3] * 256 + 359) / 360;
    if (!threshold || s->qualified || s->accumulated >= threshold) {
      s->qualified = true; accepted = true;
      s->filtered = (uint8_t)(s->filtered + delta);
      s->position = (uint8_t)(s->position + delta);
      s->direction = direction; s->moving = true;
      s->motion_ms = now; s->hold_remainder = 0;
    }
  }
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
