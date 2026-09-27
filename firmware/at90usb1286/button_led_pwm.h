#pragma once
#include <stdint.h>
void button_led_pwm_init();
void button_led_pwm_set(uint8_t index, uint8_t duty);
void button_led_pwm_clear();
uint8_t button_led_pwm_get(uint8_t index);
