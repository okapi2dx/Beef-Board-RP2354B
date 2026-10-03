#pragma once

#include <stdbool.h>
#include <stdint.h>

// J42 (UART A) drives the turntable; J43 (UART B) drives the center bar.
void phoenixwan_uart_init(void);
void phoenixwan_uart_task(uint32_t now, const uint8_t *config,
                          int8_t direction, uint32_t motion_age_ms,
                          uint16_t motion_interval_ms,
                          bool enabled);
void phoenixwan_uart_adjust_turntable_color(uint8_t channel, bool increase);
void phoenixwan_uart_adjust_centerbar_color(uint8_t channel, bool increase);
