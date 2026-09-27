#pragma once

#include <stdbool.h>
#include <stdint.h>

// J42 (UART A) drives the turntable; J43 (UART B) drives the center bar.
void phoenixwan_uart_init(void);
void phoenixwan_uart_task(uint32_t now, const uint8_t *config,
                          int8_t direction, uint32_t motion_age_ms,
                          uint16_t motion_interval_ms,
                          uint8_t bar_level, bool host_active);
