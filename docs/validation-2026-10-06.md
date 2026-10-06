# V1.00 validation — 2026-10-06

The updated release includes the simple light interface, dark mode tooltips, revised chatter history/detail controls, raw-count turntable deadzone, multiplier sensitivity and the digital B13/B14 controller monitor.

Current configuration versions are AT90USB1286 47 and RP2354B 45. Deadzone is 0–50 encoder counts. Sensitivity is 0.000–2.000 in 0.001 increments, reset to 1.000. Factory hold is 0 ms on both boards. Digital mode does not apply a separate preset.

Confirmed regressions fixed during this audit:

- RP diagnostics omitted digital turntable buttons. The app now derives B13/B14 from accepted, delayed direction, including older firmware, and RP adds those bits to digital diagnostics while preserving raw physical inputs.
- AT90 factory reset inherited a legacy 133 ms hold while the app reset command set 0 ms. The shared reset path now sets and saves 0 ms.
- AT90 hardware button/turntable HSV shortcuts lost instantaneous direction after the encoder refactor. Qualified movement is passed to the shortcut logic; stopping does not keep adjusting colors during the hold interval.

Added regression tests fail before these fixes and pass afterward. Existing changes also preserve button guards during unrelated AT90 settings saves and align the two boards' debounce and turntable state machines.

Validation passed:

- App regression suite and Svelte/TypeScript check (0 errors, 0 warnings).
- All 14 desktop tests and version synchronization.
- Firmware host regression suite, including real production AT90 reset/update functions and RP diagnostic integration.
- All 2,001 sensitivity values, both directions, fractional carry, zero disabling, raw-count deadzone and AT90/RP parity.
- Browser tests with mock HID reports on both boards: B13/B14 states, 1P/2P label placement at widths 980/800, themes, multiplier/deadzone saves and all six tabs' layout.
- Waveform focus, detail, dragging, fixed detail background, new-input reset and unchanged-report history preservation.
- AT90 and RP firmware builds and portable Windows application packaging.
- HEX checksums, EOF, unique addresses and application/boot region bounds; UF2 block magic, numbering, family flags, flash bounds and nonoverlap.
- Packaged UI and desktop files match the build/source; bundled RP firmware matches the release UF2.

Physical USB operation, contact/encoder signals, host game behavior and actual firmware flashing/reboot were not tested. Existing Vite chunk-size and mixed-import warnings remain nonfatal.

The release assets keep the existing V1.00 names. The tag is updated to the source commit used for this release. SHA256SUMS-V1.00.txt identifies the three downloadable binaries. Replacing V1.00 does not change the app version, so users should download the updated files directly from the release page.
