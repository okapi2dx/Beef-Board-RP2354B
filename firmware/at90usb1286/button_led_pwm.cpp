#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "button_led_pwm.h"

namespace { volatile uint8_t duty[11] = {}; uint8_t phase = 0; }

// Timer3: 20 kHz steps, 100 steps per PWM period = 200 Hz.
// Dedicated bit writes preserve input pullups and the RGB pins sharing ports.
ISR(TIMER3_COMPA_vect) {
  if (++phase == 100) phase = 0;
#define LED(i, port, bit) if (phase < duty[i]) port |= _BV(bit); else port &= ~_BV(bit)
  LED(0, PORTE, 1); LED(1, PORTB, 7); LED(2, PORTD, 5);
  LED(3, PORTD, 3); LED(4, PORTD, 1); LED(5, PORTD, 7);
  LED(6, PORTB, 5); LED(7, PORTC, 1); LED(8, PORTA, 3);
  LED(9, PORTA, 5); LED(10, PORTC, 3);
#undef LED
}

void button_led_pwm_init() {
  TCCR3A = 0;
  TCCR3B = 0;
  TCNT3 = 0;
  OCR3A = (F_CPU / 8 / 20000) - 1;
  TIFR3 = _BV(OCF3A);
  TIMSK3 = _BV(OCIE3A);
  TCCR3B = _BV(WGM32) | _BV(CS31);
}
void button_led_pwm_set(uint8_t index, uint8_t value) { duty[index] = value; }
uint8_t button_led_pwm_get(uint8_t index) { return duty[index]; }
void button_led_pwm_clear() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    for (uint8_t i = 0; i < 11; ++i) duty[i] = 0;
    PORTE &= ~_BV(1); PORTB &= ~(_BV(7) | _BV(5));
    PORTD &= ~(_BV(5) | _BV(3) | _BV(1) | _BV(7));
    PORTC &= ~(_BV(1) | _BV(3)); PORTA &= ~(_BV(3) | _BV(5));
  }
}
