#define CONFIG_BASE_ADDR (uint8_t*)2
#define CONFIG_VERSION_ADDR (CONFIG_BASE_ADDR + offsetof(config, version))
#define CONFIG_REVERSE_TT_ADDR (CONFIG_BASE_ADDR + offsetof(config, reverse_tt))
#define CONFIG_TT_EFFECT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_effect))
#define CONFIG_TT_DEADZONE_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_deadzone))
#define CONFIG_BAR_EFFECT_ADDR (CONFIG_BASE_ADDR + offsetof(config, bar_effect))
#define CONFIG_DISABLE_LEDS_ADDR (CONFIG_BASE_ADDR + offsetof(config, disable_leds))
#define CONFIG_TT_STATIC_HUE_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_static_hsv.h))
#define CONFIG_TT_STATIC_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_static_hsv.s))
#define CONFIG_TT_STATIC_VAL_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_static_hsv.v))
#define CONFIG_TT_SPIN_HUE_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_spin_hsv.h))
#define CONFIG_TT_SHIFT_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_shift_hsv.s))
#define CONFIG_TT_SHIFT_VAL_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_shift_hsv.v))
#define CONFIG_TT_RAINBOW_STATIC_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_rainbow_static_hsv.s))
#define CONFIG_TT_RAINBOW_REACT_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_rainbow_react_hsv.s))
#define CONFIG_TT_RAINBOW_SPIN_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_rainbow_spin_hsv.s))
#define CONFIG_TT_REACT_HUE_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_react_hsv.h))
#define CONFIG_TT_REACT_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_react_hsv.s))
#define CONFIG_TT_BREATHING_HUE_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_breathing_hsv.h))
#define CONFIG_TT_BREATHING_SAT_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_breathing_hsv.s))
#define CONFIG_TT_RATIO_ADDR (CONFIG_BASE_ADDR + offsetof(config, tt_ratio))
#define CONFIG_CONTROLLER_TYPE_ADDR (CONFIG_BASE_ADDR + offsetof(config, controller_type))
#define CONFIG_IIDX_INPUT_MODE_ADDR (CONFIG_BASE_ADDR + offsetof(config, iidx_input_mode))
#define CONFIG_SDVX_INPUT_MODE_ADDR (CONFIG_BASE_ADDR + offsetof(config, sdvx_input_mode))

#include <avr/eeprom.h>

#include "devices/iidx/iidx_rgb_manager.h"

#include "analog_button.h"
#include "beef.h"
#include "config.h"
#include "rgb_helper.h"

enum {
  MAGIC = 0xBEEF,

  DEADZONE_MAX = 255,
  DEADZONE_MIN = 0,

  RATIO_MAX = 10,
  RATIO_MIN = 1
};

config current_config;

// Default key mappings
const IIDXKeyMapping DEFAULT_IIDX_KEYS = {
  .key_codes = {
    HID_KEYBOARD_SC_S,                 // 1
    HID_KEYBOARD_SC_D,                 // 2
    HID_KEYBOARD_SC_F,                 // 3
    HID_KEYBOARD_SC_SPACE,             // 4
    HID_KEYBOARD_SC_J,                 // 5
    HID_KEYBOARD_SC_K,                 // 6
    HID_KEYBOARD_SC_L,                 // 7
    HID_KEYBOARD_SC_1_AND_EXCLAMATION, // E1/Start
    HID_KEYBOARD_SC_2_AND_AT,          // E2
    HID_KEYBOARD_SC_3_AND_HASHMARK,    // E3
    HID_KEYBOARD_SC_4_AND_DOLLAR,      // E4/Select
    HID_KEYBOARD_SC_DOWN_ARROW,        // TT -
    HID_KEYBOARD_SC_UP_ARROW           // TT +
  }
};

const SDVXKeyMapping DEFAULT_SDVX_KEYS = {
  .key_codes = {
    HID_KEYBOARD_SC_D, // BT-A
    HID_KEYBOARD_SC_F, // BT-B
    HID_KEYBOARD_SC_J, // BT-C
    HID_KEYBOARD_SC_K, // BT-D
    HID_KEYBOARD_SC_C, // FX-L
    HID_KEYBOARD_SC_M, // FX-R
    0,
    0,
    HID_KEYBOARD_SC_ENTER // Start
  }
};

bool validate_config(const config &self) {
  if (self.button_led_fade_ms_extended > 1000) return false;
  if (self.button_led_brightness > 100 || self.button_led_invert > 1) return false;
  if (self.button_led_individual > 1 || self.tt_curve > TurntableCurve::Dynamic) return false;
  for (uint8_t i = 0; i < BUTTONS; ++i)
    if (self.button_led_level[i] > 100 || self.button_led_inverted[i] > 1) return false;
  uint16_t mapped_buttons = 0;
  for (uint8_t i = 0; i < BUTTONS; ++i) {
    if (self.button_mapping[i] >= BUTTONS) return false;
    const uint16_t bit = uint16_t(1) << self.button_mapping[i];
    if (mapped_buttons & bit) return false;
    mapped_buttons |= bit;
  }
  if (self.tt_effect >= TurntableMode::Count) {
    return false;
  }
  if (self.bar_effect >= BarMode::Count) {
    return false;
  }
  if (self.tt_deadzone < DEADZONE_MIN || self.tt_deadzone > DEADZONE_MAX) {
    return false;
  }
  if (self.tt_ratio < RATIO_MIN || self.tt_ratio > RATIO_MAX) {
    return false;
  }
  if (self.controller_type > ControllerType::IIDXPremium) {
    return false;
  }
  if (self.iidx_input_mode > InputMode::Keyboard) {
    return false;
  }
  if (self.sdvx_input_mode > InputMode::Keyboard) {
    return false;
  }
  if (self.led_refresh == 0) {
    return false;
  }
  if (self.rainbow_spin_speed == 0) {
    return false;
  }
  if (self.tt_leds == 0) {
    return false;
  }
  if (self.link_bar_effect > 1) {
    return false;
  }
  if (self.digital_tt > 1) {
    return false;
  }

  return true;
}

void config_init(config* self) {
  const auto magic = eeprom_read_word(nullptr);
  if (magic != MAGIC) {
    eeprom_write_word(nullptr, MAGIC);
    self->version = 0;
  } else {
    eeprom_read_block(self, CONFIG_BASE_ADDR, sizeof(*self));
  }

  config_update(self);
}

void config_update(config* self) {
  switch (self->version) {
    case 0:
      self->reverse_tt = 0;
      self->tt_effect = TurntableMode::Spin;
      self->version++;
    case 1:
      self->tt_deadzone = 4;
      self->version++;
    case 2:
      self->bar_effect = BarMode::HID;
      self->version++;
    case 3:
      self->disable_leds = 0;
      self->version++;
    case 4:
      self->tt_static_hsv = DEFAULT_COLOUR;
      self->version++;
    case 5:
      if (self->tt_effect >= TurntableMode::RainbowSpin) // Added new effect
        self->tt_effect = TurntableMode(uint8_t(self->tt_effect)+1);
      self->version++;
    case 6:
      self->tt_spin_hsv = DEFAULT_COLOUR;
      self->tt_shift_hsv = { 0, 255, 255 };
      self->tt_rainbow_static_hsv = { 0, 255, 255 };
      self->tt_rainbow_react_hsv = { 0, 255, 255 };
      self->tt_rainbow_spin_hsv = { 0, 255, 255 };
      self->tt_react_hsv = DEFAULT_COLOUR;
      self->tt_breathing_hsv = DEFAULT_COLOUR;
      self->version++;
    case 7:
      self->tt_ratio = 2;
      self->version++;
    case 8:
      self->controller_type = ControllerType::Default;
      self->version++;
    case 9:
      self->iidx_input_mode = InputMode::Joystick;
      self->sdvx_input_mode = InputMode::Joystick;
      self->version++;
    case 10:
      // Web config support
      self->version++;
    case 11:
      self->tt_sustain_ms = 133;
      self->version++;
    case 12:
      // Key mapping support
      self->iidx_keys = DEFAULT_IIDX_KEYS;
      self->sdvx_keys = DEFAULT_SDVX_KEYS;
      self->version++;
    case 13:
      // Added new effects
      if (self->bar_effect >= BarMode::TapeLedP1) {
        self->bar_effect = BarMode(uint8_t(self->bar_effect)+2);
      }
      self->version++;
    case 14:
      self->iidx_buttons_debounce = 0;
      self->iidx_effectors_debounce = 4;
      self->sdvx_buttons_debounce = 0;
      self->version++;
    case 15:
      self->led_refresh = 60;
      self->rainbow_spin_speed = 3;
      self->tt_leds = 24;
      self->version++;
    case 16:
      self->bar_static_hsv = DEFAULT_COLOUR;
      self->version++;
    case 17:
      // Replaced SDVX mode with three IIDX USB identity modes.
      self->version++;
    case 18:
      self->link_bar_effect = 0;
      self->version++;
    case 19:
      self->digital_tt = 0;
      self->version++;
    case 20:
      self->tt_delay_ms = 0;
      self->version++;
    case 21:
      // The old distance threshold cannot be converted to a duration.
      self->tt_deadzone = 0;
      self->version++;
    case 22:
      self->button_led_fade_ms = 0;
      self->button_led_brightness = 100;
      self->button_led_invert = 0;
      self->version++;
    case 23:
      self->button_led_individual = 0;
      for (uint8_t i = 0; i < BUTTONS; ++i) {
        self->button_led_fade[i] = self->button_led_fade_ms;
        self->button_led_level[i] = self->button_led_brightness;
        self->button_led_inverted[i] = self->button_led_invert;
      }
      self->tt_curve = TurntableCurve::Linear;
      self->version++;
    case 24:
      // Keep the legacy tail layout for EEPROM/HID compatibility with 1.11.
      // This release restores 1.10 lighting and linear turntable behavior.
      self->button_led_individual = 0;
      self->tt_curve = TurntableCurve::Linear;
      self->version++;
    case 25:
      // Extend turntable sensitivity from 6 to 10 steps. Existing divider
      // values remain unchanged, preserving the configured motion speed.
      self->version++;
    case 26:
      // Store the button LED fade in 16 bits to support up to 1000 ms.
      self->button_led_fade_ms_extended = self->button_led_fade_ms;
      self->version++;
    case 27:
      // Physical-to-logical button remapping. Start with the original layout.
      for (uint8_t i = 0; i < BUTTONS; ++i) self->button_mapping[i] = i;
      self->version++;
    case 28:
      // Reserved migration after the v28 config transport change.
      // Do not alter the user's LED preference here.
      self->version++;
    case 29:
      // Diagnostics now stream over a dedicated interrupt IN endpoint instead
      // of requiring continuous control-transfer polling from the desktop app.
      self->version++;
    case 30:
      // Convert the legacy divider into the new direct 1..10 sensitivity scale.
      // Divider 1 -> sensitivity 10, divider 2 -> sensitivity 5.
      self->tt_ratio = self->tt_ratio == 0 ? 10 : static_cast<uint8_t>((10 + self->tt_ratio - 1) / self->tt_ratio);
      if (self->tt_ratio < RATIO_MIN) self->tt_ratio = RATIO_MIN;
      if (self->tt_ratio > RATIO_MAX) self->tt_ratio = RATIO_MAX;
      self->version++;
    default: break;
  }

  eeprom_update_block(self, CONFIG_BASE_ADDR, sizeof(config));
}

void config_update_setting(uint8_t* addr, const uint8_t val) {
  eeprom_update_byte(addr, val);
}

void config_save(const config &new_config) {
  if (!validate_config(new_config)) {
    return;
  }

  if (new_config.disable_leds) {
    FastLED.clear(true);
  }

  RgbHelper::update(new_config);
  usb_handler->config_update(new_config);

  memcpy(&current_config, &new_config, sizeof(config));
  current_config.reverse_tt &= 1;
  current_config.disable_leds &= 1;
  current_config.link_bar_effect &= 1;
  current_config.digital_tt &= 1;
  eeprom_update_block(&current_config, CONFIG_BASE_ADDR, sizeof(config));
}

void set_controller_type(config &self, ControllerType mode) {
  self.controller_type = mode;
  eeprom_update_byte(CONFIG_CONTROLLER_TYPE_ADDR, static_cast<uint8_t>(self.controller_type));
}

void set_input_mode(config &self, InputMode mode) {
  switch (self.controller_type) {
    case ControllerType::Default:
    case ControllerType::IIDXEntry:
    case ControllerType::IIDXPremium:
      self.iidx_input_mode = mode;
      eeprom_update_byte(CONFIG_IIDX_INPUT_MODE_ADDR, static_cast<uint8_t>(self.iidx_input_mode));
      break;
  }
}

callback toggle_reverse_tt(config* self) {
  self->reverse_tt ^= 1;
  eeprom_write_byte(CONFIG_REVERSE_TT_ADDR, self->reverse_tt);

  update_tt_transitions(self->reverse_tt);
  IIDX::RgbManager::Turntable::reverse_tt(self->reverse_tt);

  return callback{};
}

callback cycle_tt_effects(config* self) {
  self->tt_effect = TurntableMode((uint8_t(self->tt_effect) + 1) % uint8_t(TurntableMode::Count));
  eeprom_write_byte(CONFIG_TT_EFFECT_ADDR, uint8_t(self->tt_effect));

  IIDX::RgbManager::Turntable::set_leds_off();
  IIDX::RgbManager::Turntable::force_update = true;

  return callback{};
}

callback tt_hsv_set_hue(config* self) {
  uint8_t* addr;
  HSV* h;

  switch (self->tt_effect) {
    case TurntableMode::Static:
      addr = CONFIG_TT_STATIC_HUE_ADDR;
      h = &self->tt_static_hsv;
      break;
    case TurntableMode::Spin:
      addr = CONFIG_TT_SPIN_HUE_ADDR;
      h = &self->tt_spin_hsv;
      break;
    case TurntableMode::React:
      addr = CONFIG_TT_REACT_HUE_ADDR;
      h = &self->tt_react_hsv;
      break;
    case TurntableMode::Breathing:
      addr = CONFIG_TT_BREATHING_HUE_ADDR;
      h = &self->tt_breathing_hsv;
      break;
    default:
      return callback{};
  }

  h->h += button_x.direction;

  return callback{addr, h->h};
}

callback tt_hsv_set_sat(config* self) {
  uint8_t* addr;
  HSV* s;

  switch (self->tt_effect) {
    case TurntableMode::Static:
      addr = CONFIG_TT_STATIC_SAT_ADDR;
      s = &self->tt_static_hsv;
      break;
    case TurntableMode::Shift:
      addr = CONFIG_TT_SHIFT_SAT_ADDR;
      s = &self->tt_shift_hsv;
      break;
    case TurntableMode::RainbowStatic:
      addr = CONFIG_TT_RAINBOW_STATIC_SAT_ADDR;
      s = &self->tt_rainbow_static_hsv;
      break;
    case TurntableMode::RainbowReact:
      addr = CONFIG_TT_RAINBOW_REACT_SAT_ADDR;
      s = &self->tt_rainbow_react_hsv;
      break;
    case TurntableMode::RainbowSpin:
      addr = CONFIG_TT_RAINBOW_SPIN_SAT_ADDR;
      s = &self->tt_rainbow_spin_hsv;
      break;
    case TurntableMode::React:
      addr = CONFIG_TT_REACT_SAT_ADDR;
      s = &self->tt_react_hsv;
      break;
    case TurntableMode::Breathing:
      addr = CONFIG_TT_BREATHING_SAT_ADDR;
      s = &self->tt_breathing_hsv;
      break;
    default:
      return callback{};
  }

  const auto tt1_report = button_x.direction;

  if ((s->s == 0 && tt1_report < 0) ||
      (s->s == 255 && tt1_report > 0)) // prevent looping around
    return callback{};

  s->s += tt1_report;

  return callback{addr, s->s};
}

callback tt_hsv_set_val(config* self) {
  uint8_t* addr;
  HSV* v;

  switch (self->tt_effect) {
    case TurntableMode::Static:
      addr = CONFIG_TT_STATIC_VAL_ADDR;
      v = &self->tt_static_hsv;
      break;
    case TurntableMode::Shift:
      addr = CONFIG_TT_SHIFT_VAL_ADDR;
      v = &self->tt_shift_hsv;
      break;
    case TurntableMode::RainbowStatic:
      addr = CONFIG_TT_RAINBOW_STATIC_SAT_ADDR;
      v = &self->tt_rainbow_static_hsv;
      break;
    case TurntableMode::RainbowReact:
      addr = CONFIG_TT_RAINBOW_REACT_SAT_ADDR;
      v = &self->tt_rainbow_react_hsv;
      break;
    case TurntableMode::RainbowSpin:
      addr = CONFIG_TT_RAINBOW_SPIN_SAT_ADDR;
      v = &self->tt_rainbow_spin_hsv;
      break;
    default:
      return callback{};
  }

  const auto tt1_report = button_x.direction;

  if ((v->v == 0 && tt1_report < 0) ||
      (v->v == 255 && tt1_report > 0)) // prevent looping around
    return callback{};

  v->v += tt1_report;

  return callback{addr, v->v};
}

void update_deadzone(const uint8_t deadzone) {
  eeprom_update_byte(CONFIG_TT_DEADZONE_ADDR, deadzone);

  IIDX::RgbManager::Turntable::display_tt_change(CRGB::Green,
                                                 deadzone,
                                                 DEADZONE_MAX);
}

callback increase_deadzone(config* self) {
  if (self->tt_deadzone < DEADZONE_MAX) ++self->tt_deadzone;
  update_deadzone(self->tt_deadzone);

  return callback{};
}

callback decrease_deadzone(config* self) {
  if (self->tt_deadzone > DEADZONE_MIN) --self->tt_deadzone;
  update_deadzone(self->tt_deadzone);

  return callback{};
}

void update_ratio(const uint8_t ratio) {
  eeprom_update_byte(CONFIG_TT_RATIO_ADDR, ratio);

  // Present TT ratio as TT sensitivity to the user
  IIDX::RgbManager::Turntable::display_tt_change(CRGB::Red,
                                                 ratio,
                                                 RATIO_MAX);
}

callback increase_ratio(config* self) {
  self->tt_ratio = MIN(self->tt_ratio + 1, static_cast<uint8_t>(RATIO_MAX));
  update_ratio(self->tt_ratio);

  return callback{};
}

callback decrease_ratio(config* self) {
  self->tt_ratio = MAX(self->tt_ratio - 1, static_cast<uint8_t>(RATIO_MIN));
  update_ratio(self->tt_ratio);

  return callback{};
}

callback cycle_bar_effects(config* self) {
  do {
    self->bar_effect = BarMode((uint8_t(self->bar_effect) + 1) % uint8_t(BarMode::Count));
  } while (self->bar_effect == BarMode::Placeholder1 ||
           self->bar_effect == BarMode::Placeholder3);
  eeprom_write_byte(CONFIG_BAR_EFFECT_ADDR, uint8_t(self->bar_effect));

  IIDX::RgbManager::Bar::set_leds_off();
  IIDX::RgbManager::Bar::force_update = true;

  return callback{};
}

callback toggle_disable_leds(config* self) {
  self->disable_leds ^= 1;

  eeprom_write_byte(CONFIG_DISABLE_LEDS_ADDR, self->disable_leds);

  if (self->disable_leds) {
    FastLED.clear(true);
  }

  return callback{};
}
