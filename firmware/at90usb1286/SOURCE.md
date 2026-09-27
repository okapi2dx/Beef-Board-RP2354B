AT90USB1286 firmware source was copied from the locally available
HWXLR8/beef-board checkout (its `fw/` directory) for reference and offline builds.
Upstream: https://github.com/HWXLR8/beef-board
The original firmware is
GPL-3.0; bundled third-party libraries retain their own licenses.
`FastLED/src` and LUFA files are included so this firmware
can be built offline with an installed AVR-GCC toolchain and `make`.
Compiled firmware and the bootloader HEX are intentionally excluded.

From this directory, run `make -j1` to produce `beef.hex`. Build products
are ignored. No board is flashed by the build command.
