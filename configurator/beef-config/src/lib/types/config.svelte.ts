import { ReportId } from '$lib/types/hid';
import { appState } from '$lib/types/state.svelte';
import { TurntableMode, BarMode, ControllerType, InputMode, TurntableCurve, Hsv, numberToTurntableMode, numberToBarMode, numberToControllerType, numberToInputMode, numberToTurntableCurve, turntableModeToNumber, barModeToNumber, controllerTypeToNumber, inputModeToNumber, turntableCurveToNumber } from '$lib/types/types.svelte';
import * as HIDCodes from '$lib/types/hid-codes';

const packetSize = 1024;
let lastConfigPacket: Uint8Array | undefined;

function samePacket(a: Uint8Array | undefined, b: Uint8Array): boolean {
  if (!a || a.length !== b.length) return false;
  for (let i = 0; i < b.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

export class IIDXKeyMapping {
  public main_buttons = $state<number[]>([]);
  public function_buttons = $state<number[]>([]);
  public tt_ccw = $state(0);
  public tt_cw = $state(0);
  public padding: number[];

  constructor() {
    this.main_buttons = [
      HIDCodes.HID_KEYBOARD_SC_S, // 1
      HIDCodes.HID_KEYBOARD_SC_D, // 2
      HIDCodes.HID_KEYBOARD_SC_F, // 3
      HIDCodes.HID_KEYBOARD_SC_SPACE, // 4
      HIDCodes.HID_KEYBOARD_SC_J, // 5
      HIDCodes.HID_KEYBOARD_SC_K, // 6
      HIDCodes.HID_KEYBOARD_SC_L // 7
    ];
    this.function_buttons = [
      HIDCodes.HID_KEYBOARD_SC_1_AND_EXCLAMATION, // E1/Start
      HIDCodes.HID_KEYBOARD_SC_2_AND_AT, // E2
      HIDCodes.HID_KEYBOARD_SC_3_AND_HASHMARK, // E3
      HIDCodes.HID_KEYBOARD_SC_4_AND_DOLLAR // E4/Select
    ];
    this.tt_ccw = HIDCodes.HID_KEYBOARD_SC_DOWN_ARROW; // TT-
    this.tt_cw = HIDCodes.HID_KEYBOARD_SC_UP_ARROW; // TT+
    this.padding = Array(7).fill(0);
  }
}

export class SDVXKeyMapping {
  public bt_buttons = $state<number[]>([]);
  public fx_buttons = $state<number[]>([]);
  public padding_1: number[];
  public start = $state(0);
  public padding_2: number[];

  constructor() {
    this.bt_buttons = [
      HIDCodes.HID_KEYBOARD_SC_D, // BT-A
      HIDCodes.HID_KEYBOARD_SC_F, // BT-B
      HIDCodes.HID_KEYBOARD_SC_J, // BT-C
      HIDCodes.HID_KEYBOARD_SC_K // BT-D
    ];
    this.fx_buttons = [
      HIDCodes.HID_KEYBOARD_SC_C, // FX-L
      HIDCodes.HID_KEYBOARD_SC_M // FX-R
    ];
    this.padding_1 = Array(2).fill(0);
    this.start = HIDCodes.HID_KEYBOARD_SC_ENTER; // Start
    this.padding_2 = Array(11).fill(0);
  }
}

export class Config {
  version = $state(0);
  reverse_tt = $state(false);
  tt_effect = $state(TurntableMode.Static);
  tt_deadzone = $state(0);
  bar_effect = $state(BarMode.KeySpectrumP1);
  disable_leds = $state(false);
  tt_static_hsv = $state(new Hsv(0, 0, 0));
  tt_spin_hsv = $state(new Hsv(0, 0, 0));
  tt_shift_hsv = $state(new Hsv(0, 0, 0));
  tt_rainbow_static_hsv = $state(new Hsv(0, 0, 0));
  tt_rainbow_react_hsv = $state(new Hsv(0, 0, 0));
  tt_rainbow_spin_hsv = $state(new Hsv(0, 0, 0));
  tt_react_hsv = $state(new Hsv(0, 0, 0));
  tt_breathing_hsv = $state(new Hsv(0, 0, 0));
  tt_ratio = $state(10);
  controller_type = $state(ControllerType.Default);
  iidx_input_mode = $state(InputMode.Joystick);
  sdvx_input_mode = $state(InputMode.Joystick);
  tt_sustain_ms = $state(0);
  iidx_keys = $state(new IIDXKeyMapping());
  sdvx_keys = $state(new SDVXKeyMapping());
  iidx_buttons_debounce = $state(0);
  iidx_effectors_debounce = $state(0);
  sdvx_buttons_debounce = $state(0);
  led_refresh = $state(0);
  rainbow_spin_speed = $state(1);
  tt_leds = $state(0);
  bar_leds = $state(16);
  bar_static_hsv = $state(new Hsv(0, 255, 255));
  link_bar_effect = $state(false);
  digital_tt = $state(false);
  tt_delay_ms = $state(0);
  button_led_fade_ms = $state(0);
  button_led_brightness = $state(100);
  button_led_invert = $state(false);
  button_led_individual = $state(false);
  button_led_fade = $state<number[]>(Array(11).fill(0));
  button_led_level = $state<number[]>(Array(11).fill(100));
  button_led_inverted = $state<boolean[]>(Array(11).fill(false));
  tt_curve = $state(TurntableCurve.Linear);
  button_mapping = $state<number[]>(Array.from({ length: 11 }, (_, i) => i));

  constructor(configData: DataView) {
    this.version = configData.getUint8(0);
    this.reverse_tt = configData.getUint8(1) as unknown as boolean;
    this.tt_effect = numberToTurntableMode[configData.getUint8(2)];
    this.tt_deadzone = configData.getUint8(3);
    this.bar_effect = numberToBarMode[configData.getUint8(4)];
    this.disable_leds = configData.getUint8(5) as unknown as boolean;
    this.tt_static_hsv = new Hsv(
      configData.getUint8(6),
      configData.getUint8(7),
      configData.getUint8(8)
    );
    this.tt_spin_hsv = new Hsv(
      configData.getUint8(9),
      configData.getUint8(10),
      configData.getUint8(11)
    );
    this.tt_shift_hsv = new Hsv(
      configData.getUint8(12),
      configData.getUint8(13),
      configData.getUint8(14)
    );
    this.tt_rainbow_static_hsv = new Hsv(
      configData.getUint8(15),
      configData.getUint8(16),
      configData.getUint8(17)
    );
    this.tt_rainbow_react_hsv = new Hsv(
      configData.getUint8(18),
      configData.getUint8(19),
      configData.getUint8(20)
    );
    this.tt_rainbow_spin_hsv = new Hsv(
      configData.getUint8(21),
      configData.getUint8(22),
      configData.getUint8(23)
    );
    this.tt_react_hsv = new Hsv(
      configData.getUint8(24),
      configData.getUint8(25),
      configData.getUint8(26)
    );
    this.tt_breathing_hsv = new Hsv(
      configData.getUint8(27),
      configData.getUint8(28),
      configData.getUint8(29)
    );
    this.tt_ratio = configData.getUint8(30);
    this.controller_type = numberToControllerType[configData.getUint8(31)];
    this.iidx_input_mode = numberToInputMode[configData.getUint8(32)];
    this.sdvx_input_mode = numberToInputMode[configData.getUint8(33)];

    if (this.version >= 12) {
      this.tt_sustain_ms = configData.getUint8(34);
    }

    // Read key mappings if version supports it
    let offset = 35;
    if (this.version >= 13) {
      // Read IIDX key mappings)
      for (let i = 0; i < this.iidx_keys.main_buttons.length; i++) {
        this.iidx_keys.main_buttons[i] = configData.getUint8(offset++);
      }

      for (let i = 0; i < this.iidx_keys.function_buttons.length; i++) {
        this.iidx_keys.function_buttons[i] = configData.getUint8(offset++);
      }

      this.iidx_keys.tt_ccw = configData.getUint8(offset++);
      this.iidx_keys.tt_cw = configData.getUint8(offset++);

      // Skip padding
      offset += this.iidx_keys.padding.length;

      // Read SDVX key mappings
      for (let i = 0; i < this.sdvx_keys.bt_buttons.length; i++) {
        this.sdvx_keys.bt_buttons[i] = configData.getUint8(offset++);
      }

      for (let i = 0; i < this.sdvx_keys.fx_buttons.length; i++) {
        this.sdvx_keys.fx_buttons[i] = configData.getUint8(offset++);
      }

      offset += this.sdvx_keys.padding_1.length;

      this.sdvx_keys.start = configData.getUint8(offset++);

      // Skip padding
      offset += this.sdvx_keys.padding_2.length;
    }

    if (this.version >= 15) {
      this.iidx_buttons_debounce = configData.getUint8(offset++);
      this.iidx_effectors_debounce = configData.getUint8(offset++);
      this.sdvx_buttons_debounce = configData.getUint8(offset++);
    }

    if (this.version >= 16) {
      this.led_refresh = configData.getUint8(offset++);
      this.rainbow_spin_speed = configData.getUint8(offset++);
      this.tt_leds = configData.getUint8(offset++);
    }
    if (this.version >= 17) {
      this.bar_static_hsv = new Hsv(configData.getUint8(offset++), configData.getUint8(offset++), configData.getUint8(offset++));
    }
    if (this.version >= 19) {
      this.link_bar_effect = Boolean(configData.getUint8(offset++));
    }
    if (this.version >= 20) {
      this.digital_tt = Boolean(configData.getUint8(offset++));
    }
    if (this.version >= 21) this.tt_delay_ms = configData.getUint8(offset++);
    if (this.version >= 23) {
      this.button_led_fade_ms = configData.getUint8(offset++);
      this.button_led_brightness = configData.getUint8(offset++);
      this.button_led_invert = Boolean(configData.getUint8(offset++));
    }
    if (this.version >= 24) {
      this.button_led_individual = Boolean(configData.getUint8(offset++));
      this.button_led_fade = Array.from({ length: 11 }, () => configData.getUint8(offset++));
      this.button_led_level = Array.from({ length: 11 }, () => configData.getUint8(offset++));
      this.button_led_inverted = Array.from({ length: 11 }, () => Boolean(configData.getUint8(offset++)));
      this.tt_curve = numberToTurntableCurve[configData.getUint8(offset++)] ?? TurntableCurve.Linear;
    }
    if (this.version >= 27) {
      this.button_led_fade_ms = configData.getUint16(offset, true);
      offset += 2;
    }
    if (this.version >= 28) {
      this.button_mapping = Array.from({ length: 11 }, () => configData.getUint8(offset++));
    }
    if (this.version >= 32) {
      this.bar_leds = configData.getUint8(offset++);
    }
  }
}

export async function readConfig(): Promise<Config> {
  if (!appState.device) {
    throw new Error('Device not connected');
  }

  try {
    const result = await appState.device.receiveFeatureReport(ReportId.Config);
    const configBytes = new Uint8Array(result.buffer, result.byteOffset + 1, result.byteLength - 1).slice();
    const configData = new DataView(configBytes.buffer); // Skip report id

    const version = configData.getUint8(0);
    if (version <= 10) {
      throw new Error('Firmware version is too old. Please flash the latest firmware build');
    }

    lastConfigPacket = configBytes;
    return new Config(configData);
  } catch (err) {
    throw new Error(`Failed to read config: ${err}`);
  }
}

export async function updateConfig(config: Config): Promise<void> {
  if (!appState.device) {
    throw new Error('Device not connected');
  }

  console.log("updating config")

  try {
    const configBuffer = new ArrayBuffer(packetSize);
    const configView = new DataView(configBuffer);

    configView.setUint8(0, config.version);
    configView.setUint8(1, Number(config.reverse_tt));
    configView.setUint8(2, turntableModeToNumber[config.tt_effect]);
    if (config.version >= 33) {
      if (!Number.isInteger(config.tt_deadzone) || config.tt_deadzone < 0 || config.tt_deadzone > 30)
        throw new Error('Turntable deadzone must be 0–30°');
    } else if (config.version >= 22 && (!Number.isInteger(config.tt_deadzone) || config.tt_deadzone < 0 || config.tt_deadzone > 255)) {
      throw new Error('Turntable deadzone must be 0–255 ms');
    }
    configView.setUint8(3, config.tt_deadzone);
    configView.setUint8(4, barModeToNumber[config.bar_effect]);
    configView.setUint8(5, Number(config.disable_leds));
    let [h, s, v] = config.tt_static_hsv.toHid();
    configView.setUint8(6, h);
    configView.setUint8(7, s);
    configView.setUint8(8, v);
    [h, s, v] = config.tt_spin_hsv.toHid();
    configView.setUint8(9, h);
    configView.setUint8(10, s);
    configView.setUint8(11, v);
    [h, s, v] = config.tt_shift_hsv.toHid();
    configView.setUint8(12, h);
    configView.setUint8(13, s);
    configView.setUint8(14, v);
    [h, s, v] = config.tt_rainbow_static_hsv.toHid();
    configView.setUint8(15, h);
    configView.setUint8(16, s);
    configView.setUint8(17, v);
    [h, s, v] = config.tt_rainbow_react_hsv.toHid();
    configView.setUint8(18, h);
    configView.setUint8(19, s);
    configView.setUint8(20, v);
    [h, s, v] = config.tt_rainbow_spin_hsv.toHid();
    configView.setUint8(21, h);
    configView.setUint8(22, s);
    configView.setUint8(23, v);
    [h, s, v] = config.tt_react_hsv.toHid();
    configView.setUint8(24, h);
    configView.setUint8(25, s);
    configView.setUint8(26, v);
    [h, s, v] = config.tt_breathing_hsv.toHid();
    configView.setUint8(27, h);
    configView.setUint8(28, s);
    configView.setUint8(29, v);
    configView.setUint8(30, config.tt_ratio);
    configView.setUint8(31, controllerTypeToNumber[config.controller_type]);
    configView.setUint8(32, inputModeToNumber[config.iidx_input_mode]);
    configView.setUint8(33, inputModeToNumber[config.sdvx_input_mode]);

    // Write tt_sustain_ms if version supports it
    if (config.version >= 12) {
      configView.setUint8(34, config.tt_sustain_ms);
    }

    // Write key mappings if version supports it
    let offset = 35;
    if (config.version >= 13) {
      // Write IIDX key mappings
      for (let i = 0; i < config.iidx_keys.main_buttons.length; i++) {
        configView.setUint8(offset++, config.iidx_keys.main_buttons[i]);
      }

      for (let i = 0; i < config.iidx_keys.function_buttons.length; i++) {
        configView.setUint8(offset++, config.iidx_keys.function_buttons[i]);
      }

      configView.setUint8(offset++, config.iidx_keys.tt_ccw);
      configView.setUint8(offset++, config.iidx_keys.tt_cw);

      // Skip padding
      offset += config.iidx_keys.padding.length;

      // Write SDVX key mappings
      for (let i = 0; i < config.sdvx_keys.bt_buttons.length; i++) {
        configView.setUint8(offset++, config.sdvx_keys.bt_buttons[i]);
      }

      for (let i = 0; i < config.sdvx_keys.fx_buttons.length; i++) {
        configView.setUint8(offset++, config.sdvx_keys.fx_buttons[i]);
      }

      // Skip padding
      offset += config.sdvx_keys.padding_1.length;

      configView.setUint8(offset++, config.sdvx_keys.start);

      // Skip padding
      offset += config.sdvx_keys.padding_2.length;
    }

    if (config.version >= 15) {
      configView.setUint8(offset++, config.iidx_buttons_debounce);
      // Keep the legacy packet field so older firmware also uses one setting.
      configView.setUint8(offset++, config.iidx_buttons_debounce);
      configView.setUint8(offset++, config.sdvx_buttons_debounce);
    }

    if (config.version >= 16) {
      configView.setUint8(offset++, config.led_refresh);
      configView.setUint8(offset++, config.rainbow_spin_speed);
      configView.setUint8(offset++, config.tt_leds);
    }
    if (config.version >= 17) {
      for (const component of config.bar_static_hsv.toHid()) {
        configView.setUint8(offset++, component);
      }
    }
    if (config.version >= 19) {
      configView.setUint8(offset++, Number(config.link_bar_effect));
    }
    if (config.version >= 20) {
      configView.setUint8(offset++, Number(config.digital_tt));
    }
    if (config.version >= 21) {
      if (!Number.isInteger(config.tt_delay_ms) || config.tt_delay_ms < 0 || config.tt_delay_ms > 255)
        throw new Error('Turntable delay must be 0–255 ms');
      configView.setUint8(offset++, config.tt_delay_ms);
    }

    if (config.version >= 23) {
      const fadeMax = config.version >= 27 ? 1000 : 255;
      if (!Number.isInteger(config.button_led_fade_ms) || config.button_led_fade_ms < 0 || config.button_led_fade_ms > fadeMax)
        throw new Error(`Button LED fade must be 0–${fadeMax} ms`);
      if (!Number.isInteger(config.button_led_brightness) || config.button_led_brightness < 0 || config.button_led_brightness > 100)
        throw new Error('Button LED brightness must be 0–100%');
      configView.setUint8(offset++, Math.min(config.button_led_fade_ms, 255));
      configView.setUint8(offset++, config.button_led_brightness);
      configView.setUint8(offset++, Number(config.button_led_invert));
    }
    if (config.version >= 24) {
      if (config.button_led_fade.length !== 11 || config.button_led_level.length !== 11 || config.button_led_inverted.length !== 11)
        throw new Error('Button LED settings must contain 11 entries');
      if (config.button_led_fade.some((v) => !Number.isInteger(v) || v < 0 || v > 255) || config.button_led_level.some((v) => !Number.isInteger(v) || v < 0 || v > 100))
        throw new Error('Invalid per-button LED setting');
      configView.setUint8(offset++, Number(config.button_led_individual));
      for (const value of config.button_led_fade) configView.setUint8(offset++, value);
      for (const value of config.button_led_level) configView.setUint8(offset++, value);
      for (const value of config.button_led_inverted) configView.setUint8(offset++, Number(value));
      configView.setUint8(offset++, turntableCurveToNumber[config.tt_curve]);
    }
    if (config.version >= 27) {
      configView.setUint16(offset, config.button_led_fade_ms, true);
      offset += 2;
    }
    if (config.version >= 28) {
      if (config.button_mapping.length !== 11 ||
          config.button_mapping.some((v) => !Number.isInteger(v) || v < 0 || v >= 11))
        throw new Error('Button mapping entries must be valid button indices');
      for (const value of config.button_mapping) configView.setUint8(offset++, value);
    }
    if (config.version >= 32) {
      if (!Number.isInteger(config.bar_leds) || config.bar_leds < 1 || config.bar_leds > 255)
        throw new Error('Center bar LED count must be 1–255');
      configView.setUint8(offset++, config.bar_leds);
    }
    // WebHID feature reports must match the report length advertised by the
    // connected firmware. `configBuffer` is intentionally oversized while we
    // serialize, so only send the bytes that belong to this config version.
    const data = new Uint8Array(configBuffer, 0, offset);
    if (samePacket(lastConfigPacket, data)) return;
    await appState.device.sendFeatureReport(ReportId.Config, data);
    lastConfigPacket = data.slice();
  } catch (err) {
    throw new Error(`Failed to update config: ${err}`);
  }
}
