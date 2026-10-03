# Beef Board RP2354B firmware (V1.00)

This is native RP2354B firmware built with the Pico SDK. It preserves the Beef Board HID configuration and controller behavior. The GPIO mapping was checked against the RP2354B PCB and matching schematic on 2026-09-25. The later routing cleanup keeps all footprints and vias in place.

## Download / ダウンロード

配布用ファームウェアはこのフォルダーに直接置かず、[V1.00リリースページ](https://github.com/okapi2dx/Beef-Board-RP2354B/releases/tag/V1.00)に掲載しています。使っている基板に合ったファイルを選んでください。

| 基板 | 配布ファイル | 書き込み方法 |
|---|---|---|
| RP2354B | [`Beef-Board-RP2354B.uf2`](https://github.com/okapi2dx/Beef-Board-RP2354B/releases/download/V1.00/Beef-Board-RP2354B.uf2) | BOOTSELモードのUSBドライブへコピー |
| AT90USB1286 | [`Beef-Board-AT90USB1286.hex`](https://github.com/okapi2dx/Beef-Board-RP2354B/releases/download/V1.00/Beef-Board-AT90USB1286.hex) | Windows ConfiguratorでDFU書き込み |

Release firmware downloads are hosted on GitHub Releases. This directory contains source code, build instructions and tests; generated UF2, HEX and ELF files are not tracked here. For a debug ELF, build the firmware locally.

## Source contents

- `src/` and `CMakeLists.txt`: complete RP2354B build sources. Pico SDK and its TinyUSB submodule are external dependencies.
- `at90usb1286/`: AT90USB1286 firmware sources and build instructions.
- `../configurator/`: complete Configurator source with RP2354B detection, settings access, desktop USB permission, BOOTSEL instructions and the app's bundled UF2.

## Implemented

- 11 button inputs with GPIO interrupt sampling and immediate press/release detection. B1–B7 have separate press and release debounce settings from 0.0 to 16.0 ms in 0.1 ms steps. The press window rejects brief OFF edges; the release window rejects brief ON edges, including ON pulses that end after the window expires. A short tap's OFF is applied when the press window expires if it remains OFF. Button mapping, keyboard/joystick HID reports and diagnostics remain supported.
- 11 button LED outputs with the original brightness, fade and host-lighting logic. Software PWM runs at 100 Hz.
- Encoder 1 turntable handling on GP21/GP20. Encoder 2 pins GP3/GP2 are initialized but have no behavior in the original firmware either.
- Two WS2812 outputs on GP1/GP0, using PIO and nonblocking DMA, with 1–255 LEDs on each output. J42 (UART A, GP18/GP19) drives a Phoenixwan turntable ring and J43 (UART B, GP22/GP23) drives a Phoenixwan center bar at 115200 baud.
- HID configuration reports and settings journaled across the last two 4 KB sectors of the RP2354B's 2 MB flash. Both settings sectors are excluded from the linked program region. The prior single-sector format is imported in place.
- HID bootloader command enters the RP2350 ROM BOOTSEL USB mode. The restart command uses the watchdog reset.

The following GPIO mapping comes from the PCB/schematic pair:

| Function | GPIO, in B1..B11 order |
|---|---|
| Buttons | 46, 44, 43, 36, 34, 32, 31, 7, 6, 24, 17 |
| Button LEDs | 47, 45, 33, 35, 42, 30, 29, 4, 5, 26, 25 |

## Phoenixwan UART lighting (V1.00)

Connect the ring's RX to J42 TX and its TX to J42 RX; connect the center bar's RX to J43 TX and its TX to J43 RX. Use a shared ground and confirm the peripheral voltage before connecting. The ring and bar each perform the initialization exchange described by the [turntable](https://github.com/veroxzik/djdao-documentation/blob/main/iidx/phoenixwan/turntable-control.md) and [center bar](https://github.com/veroxzik/djdao-documentation/blob/main/iidx/phoenixwan/center-light-bar.md) notes. UART receive processing is bounded so the USB controller remains responsive.

GP18/GP19 (J42) and GP22/GP23 (J43) use `GPIO_FUNC_UART_AUX` (function 11) for UART TX/RX. Function 2 selects CTS/RTS on these pins and cannot carry the intended serial data. This firmware corrects that function selection without changing the PCB pin mapping. Physical UART operation remains unverified.

The Configurator's RP2354B-only **Use legacy Phoenixwan** switch enables or stops UART lighting control for the old Phoenixwan ring and center bar. It is saved in the spare byte of the firmware's flash settings record; The enable flag is checksum-protected in flash record format 3; format 2 records remain readable and migrate automatically. Existing records default to enabled to preserve prior behavior. Disabling UART control cannot send an off command, so peripherals may keep their last LED state until changed or reset.

The ring uses native modes 1–7 for the Configurator's Static, Spin, Shift, Rainbow Static, Rainbow Reactive, Reactive and Breathing effects. In legacy mode, the Configurator labels these **Solid, Rotation, Shift, Fixed Rainbow, Rotating Rainbow, Trigger and Breath**. It shows only the HSB controls supported by each mode; each −/+ action sends one relative adjustment command. Spin and synchronous rainbow receive motion direction; synchronous rainbow also receives an estimated speed. The legacy center bar is presented as a fixed light with direct HSB −/+ controls and a selectable count of Off or centered 2–16 LEDs. WS2812 outputs continue to use the complete Configurator effects.

The published UART command list has relative H/S/B steps but no absolute color query, color reset or step size. The ring and center-bar −/+ controls send relative commands directly, and their current absolute colors cannot be displayed or guaranteed after power cycling or peripheral reset. The protocol also has no ring off or direct HID RGB command, center-bar mode selection, moving pattern or per-pixel RGB command. For exact output use the board's WS2812 outputs. Physical Phoenixwan hardware has not yet been connected for validation.

## Build

Install the Raspberry Pi Pico SDK with submodules, CMake, Ninja and an Arm embedded GCC toolchain. This build used Pico SDK commit `079c6f3` and `arm-none-eabi-gcc` 10.3.1. Set `PICO_SDK_PATH` to the SDK root, then run from this directory:

```powershell
cmake -S . -B build -G Ninja -DPICO_TOOLCHAIN_PATH="C:/path/to/arm-toolchain/bin"
cmake --build build --target beef_board_rp2354b
```

The CMake configuration targets the RP2350B pinout, Arm core and 2 MB flash size. The output UF2 is `build/beef_board_rp2354b.uf2`.

The V1.00 release UF2 includes the GPIO interrupt and debounce changes. Measuring physical input latency still requires a board and host.

## Flashing and current limits

For first flashing, short J61 (BOOTSEL to GND), reset the board through J60 or reconnect power, then remove the J61 short after the ROM USB drive appears. Copy the UF2 to that drive. Use the USB device connector J1.

This image was compiled and checked with `picotool info`; it has not been run on a physical board. Confirm USB enumeration, all buttons/LEDs, encoder direction and RGB color order on hardware before relying on it. The two analog inputs are not sampled. The board's second encoder also remains unused by the controller logic.

The firmware identifies itself as `FEED:0003` and board type code `3` in the Beef Board information report. The included Configurator uses the same board code and PID. Its active firmware choices are RP2354B and legacy AT90; unsupported devices and flashing paths are excluded. The Configurator can read and write RP2354B settings and guide manual UF2 flashing; it does not write the UF2 to the ROM USB drive automatically. HID gamepad and keyboard reports are present independently of that application.

The Configurator source passed `svelte-check` with no errors or warnings and its static build included the UF2. USB operation and flashing still require a physical RP2354B board to verify.

RP2354B's [48 GPIO and stacked 2 MB flash](https://www.raspberrypi.com/products/rp2350/) are documented by Raspberry Pi. The [Pico SDK](https://github.com/raspberrypi/pico-sdk) supplies the RP2350 startup, ROM boot and TinyUSB integration.

## V1.00 fixes and validation

- Turntable hold expires relative to the last real movement (100 axis counts/s while holding). Analog deadzone uses 0–30 degrees; digital TT follows raw direction without the analog gate. Delay buffers both movement and releases for 0–255 ms.
- Keyboard reports include TT-/TT+ and deduplicate key codes. Pending digital TT presses survive a busy USB endpoint. Encoder edges are collected by GPIO interrupts instead of 1 ms polling.
- Spin, Shift, rainbow, Reactive, Breathing, key spectrum, tape and linked bar effects are implemented. Configured refresh is 1–60 Hz. DMA keeps LED transmission from blocking input scanning. Shrinking LED counts clears the old tail.
- Diagnostics expose physical buttons, separate raw/filtered/output axes, the encoder interval and actual PWM output levels. The Configurator maps buttons once and uses RP2354B PWM telemetry for button LEDs. RGB previews remain local simulations, not measured LED feedback.
- Settings are validated before staging. A two-sector journal retains a committed record while the other sector is erased; reset/restart waits for a successful save. The app serializes rapid edits and waits for queued settings before sending reset/boot commands.
- The desktop reconnect path requests permission for a newly selected Entry/Premium USB identity. Firmware-version reports support both current and legacy packet lengths.

Run the host regression suite from the repository root with Python 3 and host GCC:

```powershell
python firmware/tests/run_tests.py
```

It covers the production input/lighting/storage logic and extracted hardware-adapter functions with mocked USB, GPIO, DMA and flash. Cases include wraparound, all delay values, USB backpressure, 255 pixels, tail clearing, power loss during sector rollover, retries, legacy settings import, and sequence rollover. App tests run with `npm --prefix configurator/beef-config test` and desktop tests with `npm --prefix configurator/desktop test`.

These checks do not replace physical-board validation of USB enumeration, GPIO/encoder signals, WS2812 timing and persistent settings after an actual power cycle. Update both the UF2 and Configurator to use this revision together.

The V1.00 validation run also passed Svelte type checking, web/desktop builds, all app regression suites, 13 desktop unit tests, and headless UI checks for turntable feedback, button mapping, interrupt diagnostics and RP2354B settings/UF2/BOOTSEL. The packaged Electron app started successfully and served the exact matching UF2 through its local protocol. UI checks use mocked HID devices and do not flash hardware.
