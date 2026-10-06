#pragma once
#include <stdint.h>
#include <stdbool.h>

// Gate signed valid quadrature transitions before sensitivity. Retain direction across
// small encoder reversals and uneven event spacing. A one-second pause
// starts a new gesture. Pending distance is net displacement, not path length.
typedef struct {
  int8_t direction;
  int32_t pending;
  uint32_t last_motion;
} beef_encoder_gate_t;

static inline bool beef_encoder_gate_reversing(const beef_encoder_gate_t *s) {
  return s->direction && s->pending * s->direction < 0;
}

static inline int32_t beef_encoder_gate_update(beef_encoder_gate_t *s,
                                            int32_t delta, uint32_t now,
                                            uint8_t counts) {
  if (!delta) return 0;
  if ((uint32_t)(now - s->last_motion) > 1000u) {
    s->direction = 0; s->pending = 0;
  }
  s->last_motion = now;
  if (!counts) {
    s->direction = delta > 0 ? 1 : -1; s->pending = 0;
    return delta;
  }
  const int32_t threshold = (uint32_t)counts + 1u;
  s->pending += delta;
  if (s->direction && s->pending * s->direction > 0) {
    const int32_t accepted = s->pending;
    s->pending = 0;
    return accepted;
  }
  const int32_t distance = s->pending < 0 ? -s->pending : s->pending;
  if (distance < threshold) return 0;
  s->direction = s->pending > 0 ? 1 : -1;
  // Discard the blocked distance; only movement beyond the boundary passes.
  const int32_t accepted = s->direction * (distance - threshold + 1);
  s->pending = 0;
  return accepted;
}
