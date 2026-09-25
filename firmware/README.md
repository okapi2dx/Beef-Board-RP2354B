# Beef Board V2 RP2354B firmware (prototype 0.1)

This is native RP2354B firmware built with the Pico SDK. It preserves the Beef Board HID configuration and controller behavior. The GPIO mapping was checked against the RP2354B PCB and matching schematic on 2026-09-25. The later routing cleanup keeps all footprints and vias in place.

## Included

- `Beef-Board-RP2354B.uf2`: RP2350 Arm image for BOOTSEL drag-and-drop flashing.
- `Beef-Board-RP2354B.elf`: matching debug image.
- `src/` and `CMakeLists.txt`: complete build sources. Pico SDK and its TinyUSB submodule are external dependencies.
- `../configurator/`: complete Configurator source with RP2354B detection, settings access, desktop USB permission, BOOTSEL instructions and the bundled UF2.

## Implemented

- 11 button inputs with the original debounce, button mapping, keyboard/joystick HID reports and diagnostics.
- 11 button LED outputs with the original brightness, fade and host-lighting logic. Software PWM runs at 100 Hz.
- Encoder 1 turntable handling on GP21/GP20. Encoder 2 pins GP3/GP2 are initialized but have no behavior in the original firmware either.
- Two WS2812 outputs on GP1/GP0, using PIO. UART0 on GP18/GP19 and UART1 on GP22/GP23 initialize at 115200 baud. No UART command protocol existed in the source firmware.
- HID configuration reports and settings stored in the last 4 KB sector of the RP2354B's 2 MB flash. The settings sector is outside the linked program region.
- HID bootloader command enters the RP2350 ROM BOOTSEL USB mode. The restart command uses the watchdog reset.

The following GPIO mapping comes from the PCB/schematic pair:

| Function | GPIO, in B1..B11 order |
|---|---|
| Buttons | 46, 44, 43, 36, 34, 32, 31, 7, 6, 24, 17 |
| Button LEDs | 47, 45, 33, 35, 42, 30, 29, 4, 5, 26, 25 |

## Build

Install the Raspberry Pi Pico SDK with submodules, CMake, Ninja and an Arm embedded GCC toolchain. This build used Pico SDK commit `079c6f3` and `arm-none-eabi-gcc` 10.3.1. Set `PICO_SDK_PATH` to the SDK root, then run from this directory:

```powershell
cmake -S . -B build -G Ninja -DPICO_TOOLCHAIN_PATH="C:/path/to/arm-toolchain/bin"
cmake --build build --target beef_board_rp2354b
```

The CMake configuration targets the RP2350B pinout, Arm core and 2 MB flash size. The output UF2 is `build/beef_board_rp2354b.uf2`.

## Flashing and current limits

For first flashing, short J60 (BOOTSEL to GND), reset the board through J61 or reconnect power, then remove the J60 short after the ROM USB drive appears. Copy the UF2 to that drive. Use the USB device connector J1.

This image was compiled and checked with `picotool info`; it has not been run on a physical board. Confirm USB enumeration, all buttons/LEDs, encoder direction and RGB color order on hardware before relying on it. The two analog inputs are not sampled. The board's second encoder also remains unused by the controller logic.

The firmware identifies itself as `FEED:0003` and board type code `3` in the Beef Board information report. The included Configurator uses the same board code and PID. Its active firmware choices are RP2354B and legacy AT90; unsupported devices and flashing paths are excluded. The Configurator can read and write RP2354B settings and guide manual UF2 flashing; it does not write the UF2 to the ROM USB drive automatically. HID gamepad and keyboard reports are present independently of that application.

The Configurator source passed `svelte-check` with no errors or warnings and its static build included the UF2. USB operation and flashing still require a physical RP2354B board to verify.

RP2354B's [48 GPIO and stacked 2 MB flash](https://www.raspberrypi.com/products/rp2350/) are documented by Raspberry Pi. The [Pico SDK](https://github.com/raspberrypi/pico-sdk) supplies the RP2350 startup, ROM boot and TinyUSB integration.
