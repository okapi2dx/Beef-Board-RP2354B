#pragma once
#include "encoder_position.h"

class Axis {
public:
  virtual void poll() = 0;
  virtual uint8_t get() const = 0;
};

class AnalogAxis : public Axis {
public:
  explicit AnalogAxis(uint8_t pin);

  void poll() override;
  uint8_t get() const override;

private:
  uint8_t pin;
  uint8_t position{};
};

class QeAxis : public Axis {
public:
  QeAxis(volatile uint8_t* PIN, uint8_t a_pin, uint8_t b_pin);

  void poll() override;
  uint8_t get() const override;
  uint32_t transitions() const { return transition_count; }
  uint16_t last_interval() const { return transition_interval; }

private:
  volatile uint8_t* PIN;
  uint8_t a_pin;
  uint8_t b_pin;
  uint8_t prev{};
  EncoderPosition position;
  uint32_t transition_count{};
  uint16_t transition_interval{};
  uint32_t last_transition_time{};
};

extern int8_t tt_transitions[4][4];
extern AnalogAxis analog_x;
extern AnalogAxis analog_y;
extern QeAxis tt_x;
extern QeAxis tt_y;
