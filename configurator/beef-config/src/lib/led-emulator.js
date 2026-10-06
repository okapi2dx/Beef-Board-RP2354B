// Local LED model: 24 turntable pixels, 16 bar pixels. Uniform ring effects
// use the optional RGB snapshot carried by the existing diagnostic stream.
const black = '#000000';
/** @param {number} x @param {number} n */
const wrap = (x, n) => ((x % n) + n) % n;
// LED output bytes represent PWM intensity, whereas CSS rgb() uses encoded
// sRGB. Encode the approximate linear intensity for display; do not alter the
// received bytes or the firmware output. LED/screen calibration can still vary.
// Transfer function: https://www.w3.org/Graphics/Color/srgb
const displayChannel = Array.from({length:256}, (_,i) => {
  const linear = i / 255;
  return Math.round(255 * (linear <= 0.0031308 ? 12.92 * linear
    : 1.055 * Math.pow(linear, 1 / 2.4) - 0.055));
});
/** @param {number[]} rgb */
export function ledRgbToCss(rgb) {
  return `rgb(${rgb.map(value => displayChannel[Math.round(Math.max(0,Math.min(255,value)))]).join(' ')})`;
}
/**
 * @param {{h: number, s: number, v: number}} c
 * @param {number} [h]
 * @param {number} [v]
 */
function color(c, h = c.h, v = c.v) {
  const l = v * (1 - c.s / 200);
  const s = l === 0 || l === 100 ? 0 : (v - l) / Math.min(l, 100 - l) * 100;
  return `hsl(${wrap(h,360)} ${s}% ${l}%)`;
}
export function createLedEmulator() {
  let time = 0, spin = 0, rainbow = 0, react = 0, hue = 0;
  let spinRemainder = 0, rainbowRemainder = 0, lastDirection = 0;
  let lastButtons = 0, level = 0, guard = -Infinity, decay = 0;
  let lastMotion = -Infinity, targetVelocity = 0, velocity = 0;
  /** @type {import('$lib/types/hid').Diagnostics | null} */
  let previousInput = null;
  /** @type {number | null} */
  let previousPosition = null;
  let sampleTime = 0;
  const released = Array(11).fill(-Infinity);
  const wasOn = Array(11).fill(false);
  /**
   * @param {any} cfg
   * @param {import('$lib/types/hid').Diagnostics | null} input
   * @param {number} dt
   * @param {boolean} [useLedTelemetry]
   * @param {number} [displayButtons] Mapped destinations for the monitor preview.
   */
  return (cfg, input, dt, useLedTelemetry = false, displayButtons) => {
    dt = Math.max(0, dt);
    time += dt;
    const direction = (input?.direction ?? 0) * (cfg.reverse_tt ? 1 : -1);
    const count = Math.max(1, Math.min(255, cfg.tt_leds ?? 24));
    if (useLedTelemetry) {
      // RP2354B integrates continuous phase in beef_lighting_update.
      spin = wrap(spin + dt / (direction ? 25 : 50) * (direction === -1 ? -1 : 1), 12);
      rainbow = wrap(rainbow + dt / (direction ? 2 : 3) * (direction === -1 ? -1 : 1), 256);
    } else {
      // AVR SpinPattern resets its ticker on direction changes and advances
      // whole hue/pixel steps, including truncation of LED-count durations.
      if (direction !== lastDirection) spinRemainder = rainbowRemainder = 0;
      else {
        spinRemainder += dt;
        rainbowRemainder += dt;
      }
      const spinDuration = Math.max(1, Math.min(255, Math.floor((direction ? 25 : 50) * 24 / count)));
      const rainbowDuration = direction ? 2 : 3;
      const sign = direction === -1 ? -1 : 1;
      spin = wrap(spin + Math.floor(spinRemainder / spinDuration) * sign, Math.ceil(count / 2));
      rainbow = wrap(rainbow + Math.floor(rainbowRemainder / rainbowDuration) * sign, 256);
      spinRemainder %= spinDuration;
      rainbowRemainder %= rainbowDuration;
    }
    lastDirection = direction;
    // Estimate motion from successive input packets, not individual direction pulses.
    if (input && input !== previousInput) {
      const position = input.output;
      const interval = Math.max(1, time - sampleTime);
      if (Number.isFinite(position) && previousPosition !== null) {
        const delta = wrap(position - previousPosition + 128, 256) - 128;
        if (delta) {
          const next = Math.max(-1, Math.min(1, delta / interval * 6));
          const signed = next * (cfg.reverse_tt ? 1 : -1);
          if (signed * velocity < 0) velocity = 0;
          targetVelocity = signed; lastMotion = time;
        }
      } else if (!Number.isFinite(position) && direction) {
        if (direction * velocity < 0) velocity = 0;
        targetVelocity = direction; lastMotion = time;
      }
      previousPosition = Number.isFinite(position) ? position : null;
      previousInput = input; sampleTime = time;
    }
    const age = time - lastMotion;
    if (!input || age >= 150) { targetVelocity = 0; velocity = 0; }
    const target = age <= 60 ? targetVelocity : targetVelocity * Math.max(0,1-(age-60)/90);
    // Exponential easing is independent of display refresh rate.
    velocity += (target-velocity) * (1-Math.exp(-dt/40));
    const speed = cfg.rainbow_spin_speed ?? 3;
    react = wrap(react + dt / 3 * velocity * speed, 256);
    if (direction) { guard = time; hue = direction === 1 ? 0 : 180; }
    const buttons = displayButtons ?? input?.buttons ?? 0;
    const physicalButtons = input?.buttons ?? 0;
    if (buttons !== lastButtons) { level = Math.min(16, level + 1); decay = time; }
    else if (time - decay >= 30) { level = Math.max(0, level - Math.floor((time-decay)/30)); decay = time; }
    lastButtons = buttons;
    /** @param {number} cycle */
    const wave = cycle => {
      const theta = Math.min(255, (time % cycle) / 8);
      const triangle = theta < 128 ? theta * 2 : (255 - theta) * 2;
      const x = triangle / 255;
      return 100 * (x < .5 ? 2*x*x : 1-2*(1-x)*(1-x));
    };
    const mode = cfg.tt_effect;
    const actualRgb = input?.ttRgb;
    const uniformRgb = actualRgb?.length === 3 && ['Static','Shift','Reactive','Breathing','HID'].includes(mode)
      ? ledRgbToCss(actualRgb) : null;
    /** @type {Record<string, {h: number, s: number, v: number} | undefined>} */
    const modeColors = ({Static:cfg.tt_static_hsv, Spin:cfg.tt_spin_hsv, Shift:cfg.tt_shift_hsv,
      'Rainbow Static':cfg.tt_rainbow_static_hsv,'Rainbow Reactive':cfg.tt_rainbow_react_hsv,
      'Rainbow Spin':cfg.tt_rainbow_spin_hsv,Reactive:cfg.tt_react_hsv,Breathing:cfg.tt_breathing_hsv});
    const c = modeColors[mode];
    const ring = Array.from({length:24}, (_,i) => {
      if (cfg.disable_leds || cfg.tt_leds === 0 || mode === 'Off') return black;
      if (uniformRgb) return uniformRgb;
      if (mode === 'HID') return color({h:0,s:0,v:100},0,wave(2048));
      if (!c) return black;
      if (mode === 'Spin') {
        const pixel = Math.floor(i * count / 24);
        const on = useLedTelemetry ? (pixel * 24 / count | 0) % 12 === Math.floor(spin)
          : count <= 24 ? pixel === spin || pixel === spin + Math.floor(count / 2)
          : i === Math.floor(spin * 24 / count) || i === Math.floor((spin + Math.floor(count / 2)) * 24 / count);
        return on ? color({h:c.h,s:100,v:100}) : black;
      }
      if (mode === 'Shift') return color(c, Math.floor(time/100)*360/256);
      if (mode.startsWith('Rainbow')) {
        const pos = mode === 'Rainbow Static' ? 0 : mode === 'Rainbow Reactive' ? react
          : useLedTelemetry ? rainbow*speed : wrap(rainbow*speed,256);
        return color(c, i*360/24 - pos*360/256);
      }
      if (mode === 'Reactive') return color(c,c.h+hue,time-guard<500?100:25);
      if (mode === 'Breathing') return color(c,c.h,wave(3000));
      return color(c);
    });
    // The physical ring's data direction is opposite to its monitor drawing
    // direction for the two moving rainbow effects.  Keep the light-bar's
    // established direction, and reverse only the turntable preview.
    const reverseRingPreview = mode === 'Rainbow Spin' || mode === 'Rainbow Reactive';
    const ringPreview = reverseRingPreview ? [...ring].reverse() : ring;
    let bar = Array(16).fill(black);
    if (cfg.link_bar_effect) {
      // Sample rainbow hue at each bar pixel's exact position; rounding a
      // 24-pixel ring to 16 pixels makes alternating large/small color steps.
      if (mode.startsWith('Rainbow') && c && !cfg.disable_leds && cfg.tt_leds !== 0) {
        const pos = mode === 'Rainbow Static' ? 0 : mode === 'Rainbow Reactive' ? react
          : useLedTelemetry ? rainbow*speed : wrap(rainbow*speed,256);
        bar = bar.map((_,i)=>color(c, i*360/16 - pos*360/256));
      } else bar = bar.map((_,i)=>ring[Math.floor(i*24/16)]);
    }
    else if (!cfg.disable_leds) {
      const mode = cfg.bar_effect;
      if (mode === 'Static') bar.fill(color(cfg.bar_static_hsv));
      if (mode === 'HID') bar.fill(color({h:0,s:0,v:100},0,wave(2048)));
      if (mode.startsWith('Key Spectrum')) bar = bar.map((_,i)=>i<level?color({h:0,s:100,v:100},-i*16*360/256):black);
      if (mode.startsWith('Tape LED')) bar[15-Math.floor(time/50)%16]='#ffffff';
      if (mode.endsWith('(P1)')) bar.reverse();
    }
    const buttonLevels = Array.from({length:11},(_,i)=>{
      if (useLedTelemetry && input?.ledLevels?.length === 11) return input.ledLevels[i];
      // LEDs correspond to physical output pins; input diagnostics are physical.
      const inverted = Boolean(cfg.button_led_invert) !== Boolean(cfg.button_led_individual && cfg.button_led_inverted?.[i]);
      const on = Boolean(physicalButtons & (1<<i)) !== inverted;
      if (wasOn[i] && !on) released[i]=time;
      wasOn[i]=on;
      const fade=(cfg.button_led_individual ? cfg.button_led_fade?.[i] : cfg.button_led_fade_ms) || 0;
      const brightness=(cfg.button_led_individual ? cfg.button_led_level?.[i] : cfg.button_led_brightness) ?? 100;
      return cfg.disable_leds ? 0 : brightness * (on ? 1 : fade ? Math.max(0,1-(time-released[i])/fade) : 0);
    });
    if (cfg.bar_leds === 0) bar.fill(black);
    return {ring:ringPreview,bar,buttonLevels};
  };
}
