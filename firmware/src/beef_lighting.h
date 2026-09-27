#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BEEF_MAX_LEDS 255u
typedef struct { uint8_t r,g,b; } beef_rgb_t;
typedef struct {
  uint32_t last_ms, last_motion, guard_ms, decay_ms, elapsed_ms;
  float spin, rainbow, reactive, velocity, target_velocity;
  uint16_t previous_buttons;
  uint8_t previous_position, level, reactive_hue;
  bool initialized, motion_seen, guard_seen;
} beef_lighting_t;
void beef_lighting_update(beef_lighting_t *s, const uint8_t *cfg, uint32_t now,
                          uint8_t position, int8_t direction, uint16_t buttons);
void beef_lighting_render(const beef_lighting_t *s, const uint8_t *cfg,
                          bool host_active, const uint8_t tt_rgb[3], const uint8_t bar_rgb[3],
                          beef_rgb_t ring[BEEF_MAX_LEDS], beef_rgb_t bar[BEEF_MAX_LEDS]);
bool beef_lighting_due(uint32_t now, uint32_t last_frame, uint8_t refresh);
