AT90USB1286 firmware source was copied from the locally available
HWXLR8/beef-board checkout (its `fw/` directory) for reference.
Upstream: https://github.com/HWXLR8/beef-board

LUFA and FastLED are Git submodules that point to their upstream repositories.
Their exact revisions are pinned by this repository's gitlinks. Initialize them
after cloning with:

```sh
git submodule update --init --recursive
```

The AVR firmware can then be built with an installed AVR-GCC toolchain and
`make` from this directory. The original firmware is GPL-3.0; LUFA and FastLED
retain their respective upstream licenses. Generated files in this source
directory and the bootloader HEX are intentionally excluded. Download the
release-ready `Beef-Board-AT90USB1286.hex` from the
[V1.00 release page](https://github.com/okapi2dx/Beef-Board-RP2354B/releases/tag/V1.00).

From this directory, run `make -j1` to produce `beef.hex`. Build products
are ignored. No board is flashed by the build command.
