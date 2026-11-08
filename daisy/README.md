# Twenty Engine – Daisy Patch SM edition

Same engine as the Teensy/plugin (HQ: 2× oversampled oscillators, drift 0.5, oversampled arp filter),
48 kHz, 16-sample blocks (0.33 ms).

## Build
Needs `arm-none-eabi-gcc` and [libDaisy](https://github.com/electro-smith/libDaisy) (build it once with `make` in its folder).

    cd TwentyEngineDaisy
    make LIBDAISY_DIR=/path/to/libDaisy

## Flash (the firmware runs from RAM, loaded by the Daisy bootloader)
The firmware (~157 KB) is bigger than the STM32H750's 128 KB internal flash, so it uses the Daisy bootloader:

1. **Once:** put the Patch SM in DFU mode (hold BOOT, tap RESET, release BOOT), then `make program-boot`.
2. **Every update:** tap RESET, then press BOOT within 2.5 s (the LED pulses = bootloader waiting), then `make program-dfu`.

The [Daisy Web Programmer](https://flash.daisy.audio) can do both steps in Chrome as well (bootloader, then the `.bin` from `build/`).

## Connections (Patch SM)
| What | Patch SM |
|---|---|
| iC USB (cable half 2): D− / D+ | **A8 / A9** (USB host) |
| iC USB: 5 V / GND | 5 V from the supply (the Patch SM does **not** power its host pins), GND |
| Computer (cable half 1) | the Patch SM's USB-C socket → shows up as **"Twenty Engine"** |
| Audio out L / R | OUT_L / OUT_R (Eurorack level: `OUTPUT_LEVEL` in the code scales it to about line level) |
| External input | IN_L |
| Power | ±12 V for the audio stage (e.g. 5 V → A0512S DC-DC), plus 5 V |

## Diagnostics
- LED: slow blink = waiting for the iC, steady = iC connected, fast flicker = memory saved.
- CPU load: every 2 s the firmware sends SysEx `F0 7D 54 45 <percent> F7` to the computer (see it in MIDI Monitor).
- Patch memory is saved in the QSPI flash at 7 MB (no SD card needed).
