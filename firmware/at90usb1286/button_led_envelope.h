#pragma once
#include <stdint.h>

// Brightness is a duty percentage. Inversion changes the target, before fading.
class ButtonLedEnvelope {
public:
  uint8_t update(bool on, uint32_t now, uint16_t fade, uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    if (on) {
      active = true;
      return brightness;
    }
    if (active) { active = false; released = now; fading = true; }
    const uint32_t elapsed = now - released;
    if (!fading || fade == 0 || elapsed >= fade) {
      fading = false;
      return 0;
    }
    return (static_cast<uint32_t>(brightness) * (fade - elapsed)) / fade;
  }
  void reset() { active = false; fading = false; }
private:
  bool active = false;
  bool fading = false;
  uint32_t released = 0;
};
