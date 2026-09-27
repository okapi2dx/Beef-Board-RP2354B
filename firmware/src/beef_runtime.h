#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "beef_protocol.h"

typedef struct { uint8_t position; int8_t direction; } beef_tt_sample_t;
typedef struct {
  uint8_t raw, filtered, position;
  int8_t direction, gate_direction, digital_direction;
  int32_t fraction;
  uint32_t last_ms, motion_ms, gate_ms, gate_interval, digital_motion_ms;
  uint16_t accumulated, hold_remainder;
  bool initialized, qualified, moving;
  uint8_t settings[6];
  beef_tt_sample_t history[256], previous, output;
} beef_turntable_t;
void beef_turntable_update(beef_turntable_t *s, const uint8_t *cfg,
                           int32_t transitions, uint32_t now);
void beef_keyboard_report(const uint8_t *cfg, uint16_t buttons, uint8_t keys[13]);
bool beef_config_valid(const uint8_t *cfg);
