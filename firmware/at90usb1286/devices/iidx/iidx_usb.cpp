#include "../analog_button.h"
#include "../axis.h"
#include "../beef.h"
#include "iidx_combo.h"
#include "iidx_usb.h"
#include "iidx_usb_desc.h"
#include "iidx_rgb_manager.h"
#include "../../turntable_delay.h"
#include "../../analog_turntable.h"
#include "../../turntable_gate.h"
#include "../../diagnostics.h"
#include "../../button_led_pwm.h"
#include <util/atomic.h>

namespace IIDX {
  struct USB_JoystickReport_Data_t {
    uint8_t  X;
    uint8_t  Y; // Needed for LR2 compatibility
    uint16_t Button; // bit-field representing which buttons have been pressed
  } ATTR_PACKED;

  HidReport<USB_JoystickReport_Data_t, INTERFACE_ID_Joystick, JOYSTICK_IN_EPADDR> joystick_hid_report;
  HidReport<Beef::USB_KeyboardReport_Data_t, INTERFACE_ID_Keyboard, KEYBOARD_IN_EPADDR> keyboard_hid_report;
  UsbHandler usb_handler;
  Debouncer<BUTTONS> buttons_debounce;
  Debouncer<BUTTONS> effectors_debounce;
  TurntableDelay turntable_delay;
  AnalogTurntable analog_turntable;
  TurntableGate turntable_gate;
  uint8_t delayed_position = 0;

  uint32_t current_milliseconds() {
    uint32_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { now = milliseconds; }
    return now;
  }

  void process_buttons(const int8_t tt1_report) {
    if (current_config.digital_tt) {
      switch (tt1_report) {
        case -1:
          button_state |= BUTTON_TT_NEG;
          break;
        case 1:
          button_state |= BUTTON_TT_POS;
          break;
        default:
          break;
      }
    }

    button_state = buttons_debounce.debounce(button_state, MAIN_BUTTONS_ALL);
    button_state = effectors_debounce.debounce(button_state, EFFECTORS_ALL);
  }

  bool UsbHandler::create_hid_report(USB_ClassInfo_HID_Device_t* const hid_interface_info,
                                     uint8_t* const report_id,
                                     void* report_data,
                                     uint16_t* const report_size) {
    switch (hid_interface_info->Config.ReportINEndpoint.Address) {
      case JOYSTICK_IN_EPADDR: {
        auto joystick_report = (USB_JoystickReport_Data_t*)report_data;
        *report_size = sizeof(*joystick_report);

        const auto report_buttons = remap_buttons(button_state, current_config.button_mapping);
        // Infinitas only reads buttons 1-7, 9-12,
        // so shift bits 8 and up once
        const uint8_t upper = report_buttons >> 7;
        const uint8_t lower = report_buttons & 0x7F;
        joystick_report->X = current_config.digital_tt ? 127 : delayed_position;
        joystick_report->Y = 127;
        joystick_report->Button = (upper << 8) | lower;

        return true;
      }
      case KEYBOARD_IN_EPADDR: {
        auto keyboard_report = (Beef::USB_KeyboardReport_Data_t*)report_data;
        *report_size = sizeof(*keyboard_report);

        process_keyboard(keyboard_report,
                         current_config.iidx_keys.key_codes,
                         sizeof(current_config.iidx_keys.key_codes),
                         remap_buttons(button_state, current_config.button_mapping));

        return false;
      }
      default:
        Endpoint_StallTransaction();
        return false;
    }
  }

  void UsbHandler::usb_task(const config &config) {
    switch (config.iidx_input_mode) {
      case InputMode::Joystick:
        HID_Device_USBTask(&joystick_hid_report.HID_Interface);
        break;
      case InputMode::Keyboard:
        HID_Device_USBTask(&keyboard_hid_report.HID_Interface);
        break;
    }
  }

  void UsbHandler::update(const config &config) {
    HID_Task(led_data, joystick_out_state);

    tt_x.poll();
    const auto now = current_milliseconds();
    const auto filtered = turntable_gate.update(tt_x.get(), now, config.tt_deadzone);
    // Digital TT direction must follow the encoder movement directly.
    // The filtered position is intended for the analog X-axis gate and can
    // suppress short movements before a digital button is generated.
    const auto tt1_report = button_x.poll(1,
                                          config.tt_sustain_ms,
                                          tt_x.get());
    const auto analog_position = analog_turntable.update(filtered, now,
      1, config.tt_sustain_ms, tt_x.transitions());
    const auto delayed = turntable_delay.update(now,
      {analog_position, tt1_report}, config.tt_delay_ms);
    delayed_position = delayed.position;
    process_buttons(delayed.direction);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    diagnostics.buttons = button_state;
    diagnostics.sensor_ab = ((PINF & 0x01) << 1) | ((PINF >> 1) & 0x01);
    diagnostics.raw_position = tt_x.get();
    diagnostics.filtered_position = filtered;
    diagnostics.output_position = delayed_position;
    diagnostics.direction = delayed.direction;
    diagnostics.transitions = tt_x.transitions();
    diagnostics.last_transition_ms = tt_x.last_interval();
    // LED preview is generated locally by the app; reserved report bytes remain zero.
    }

    update_button_lighting(led_data.buttons);
    RgbManager::update(tt1_report, led_data);
  }

  void UsbHandler::config_update(const config &new_config) {
    if (new_config.tt_deadzone != current_config.tt_deadzone ||
        new_config.tt_sustain_ms != current_config.tt_sustain_ms ||
        new_config.digital_tt != current_config.digital_tt) {
      const auto now = current_milliseconds();
      turntable_gate.reset(tt_x.get(), delayed_position, now);
      analog_turntable.reset(delayed_position, now);
      button_x.init(1, true, tt_x.get());
      turntable_delay.reset(now, {delayed_position, 0}, new_config.tt_delay_ms);
    }
    if (new_config.tt_effect != current_config.tt_effect) {
      RgbManager::Turntable::set_leds_off();
      RgbManager::Turntable::force_update = true;
    }
    if (new_config.bar_effect != current_config.bar_effect) {
      RgbManager::Bar::set_leds_off();
      RgbManager::Bar::force_update = true;
    }
    update_tt_transitions(new_config.reverse_tt);
    buttons_debounce.init(new_config.iidx_buttons_debounce);
    effectors_debounce.init(new_config.iidx_effectors_debounce);
  }

  void usb_init(const config &config) {
    usb_desc_init(config.controller_type);

    ::usb_handler = &usb_handler;
    get_button_combo_callback = get_button_combo;

    button_x.init(1, true, tt_x.get());
    delayed_position = tt_x.get();
    turntable_gate.reset(tt_x.get(), delayed_position, current_milliseconds());
    analog_turntable.reset(delayed_position, current_milliseconds());
    turntable_delay.reset(current_milliseconds(), {delayed_position, 0}, config.tt_delay_ms);
    buttons_debounce.init(config.iidx_buttons_debounce);
    effectors_debounce.init(config.iidx_effectors_debounce);

    update_tt_transitions(config.reverse_tt);

    RgbManager::init(config);
  }
}



