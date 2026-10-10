#pragma once
#include <stdint.h>
#include <stdbool.h>

static inline uint8_t beef_change_speed(uint8_t speed) {
  return speed < 1 ? 1 : speed > 5 ? 5 : speed;
}

typedef struct {
  uint32_t last_ms;
  uint16_t fraction;
  uint8_t hue;
  bool initialized;
} beef_color_shift_t;

// Speed 3 retains the original 100 ms/hue-step pace. Integrate increments,
// rather than scaling absolute time, so changing speed does not jump colors.
static inline uint8_t beef_color_shift_update(beef_color_shift_t *s,
                                             uint32_t now, uint8_t speed) {
  if (!s->initialized) { s->initialized = true; s->last_ms = now; return s->hue; }
  uint32_t elapsed = now - s->last_ms;
  s->last_ms = now;
  uint32_t scaled = (elapsed % 76800u) * beef_change_speed(speed) + s->fraction;
  s->hue = (uint8_t)(s->hue + scaled / 300u);
  s->fraction = scaled % 300u;
  return s->hue;
}

typedef struct {
  uint32_t last_ms;
  uint16_t phase;
  bool initialized;
} beef_breathing_t;

static inline uint16_t beef_breathing_update(beef_breathing_t *s,
                                            uint32_t now, uint8_t speed) {
  if (!s->initialized) { s->initialized = true; s->last_ms = now; return 0; }
  uint32_t elapsed = now - s->last_ms;
  s->last_ms = now;
  s->phase = (s->phase + (elapsed % 9000u) * beef_change_speed(speed)) % 9000u;
  return s->phase / 3u;
}
