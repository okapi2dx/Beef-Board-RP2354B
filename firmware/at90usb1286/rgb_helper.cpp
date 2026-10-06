#include "config.h"
#include "hid.h"
#include "rgb_helper.h"
#include "devices/iidx/iidx_rgb_manager.h"

static CRGB turntable_pixels[255];
CRGB* tt_leds = turntable_pixels;
CRGB bar_leds[255];

namespace RgbHelper {
  // Pin mapping can be found in FastLED/src/platforms/avr/fastpin_avr.h
  enum {
    BAR_DATA_PIN = 14, // C4
    TT_DATA_PIN  = 15  // C5
  };

  timer combo_timer{};
  uint32_t min_micros = 0;
  uint8_t tt_anim_normalise = 0;
  uint8_t num_tt_leds = 0;
  uint8_t num_bar_leds = 0;

  CLEDController* tt_controller;
  CLEDController* bar_controller;

  void set_refresh_rate(uint8_t rate) {
    min_micros = 1000000 / rate;
  }

  void init(const config &cfg) {
    timer_init(&combo_timer);

    tt_controller = &FastLED.addLeds<NEOPIXEL, TT_DATA_PIN>(tt_leds, num_tt_leds)
      .setDither(DISABLE_DITHER);
    bar_controller = &FastLED.addLeds<NEOPIXEL, BAR_DATA_PIN>(bar_leds, 0)
      .setDither(DISABLE_DITHER);
    update(cfg);
    FastLED.setMaxRefreshRate(0); // We have our own frame rate limiter
  }

  void update(const config &new_cfg) {
    set_refresh_rate(new_cfg.led_refresh);
    const uint8_t bar_count = new_cfg.sdvx_keys.key_codes[6];
    if (num_tt_leds != new_cfg.tt_leds || num_bar_leds != bar_count) {
      // Clear the previous length before shrinking or disabling either output.
      if (num_tt_leds) { set_rgb(tt_leds, num_tt_leds, CRGB::Black); show_tt(); }
      if (num_bar_leds) { set_rgb(bar_leds, num_bar_leds, CRGB::Black); show_bar(); }
      num_tt_leds = new_cfg.tt_leds;
      num_bar_leds = bar_count;
      tt_anim_normalise = num_tt_leds ? (24u / num_tt_leds ? 24u / num_tt_leds : 1u) : 1u;
      tt_controller->setLeds(tt_leds, num_tt_leds);
      bar_controller->setLeds(bar_leds, num_bar_leds);
      IIDX::RgbManager::init(new_cfg);
      IIDX::RgbManager::Turntable::force_update = true;
      IIDX::RgbManager::Bar::force_update = true;
    }
  }

  bool set_rgb(CRGB* leds, const uint8_t n, const rgb_light &lights) {
    return set_rgb(leds, n,
                   CRGB(lights.r, lights.g, lights.b));
  }

  bool set_rgb(CRGB* leds, const uint8_t n, const CRGB &rgb) {
    bool update = false;
    for (uint8_t i = 0; i < n; i++) {
      update |= leds[i] != rgb;
      leds[i] = rgb;
    }
    return update;
  }

  bool set_hsv(CRGB* leds, const uint8_t n, const HSV &hsv) {
    CRGB rgb{};
    hsv2rgb_spectrum(CHSV(hsv.h, hsv.s, hsv.v), rgb);
    return set_rgb(leds, n, rgb);
  }

  bool breathing(BreathingPattern &breathing_pattern,
                 CRGB* leds, const uint8_t n, const HSV &hsv) {
    const auto v = breathing_pattern.update();
    return set_hsv(leds, n, { hsv.h, hsv.s, v });
  }

  // Share same lighting state between different lights for HID standby animation
  BreathingPattern hid_standby;
  bool hid(CRGB* leds, const uint8_t n, const rgb_light &lights) {
    if (joystick_out_state.on_standby()) {
      return breathing(hid_standby, leds, n, {});
    }
    return set_rgb(leds, n, lights);
  }

  // update lighting on a timer to reduce the number of
  // computationally expensive calls to FastLED.show()
  // basically what FastLED does but without spin waiting
  bool ready_to_present() {
    if (current_config.disable_leds)
      return false;

    static uint32_t last_show = 0;
    const uint32_t now = micros();
    if (now-last_show < min_micros)
      return false;

    last_show = now;
    return true;
  }

  void show_tt() { if (num_tt_leds) tt_controller->showLeds(); }
  void show_bar() { if (num_bar_leds) bar_controller->showLeds(); }
}
