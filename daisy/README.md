# Twenty Engine – Daisy Patch SM edition (beta)

**Beta:** builds from the same engine as the plugin, Ksoloti and Teensy editions, but has not been tested on hardware yet.

Same engine as the Teensy/plugin (HQ: 2× oversampled oscillators, drift 0.5, oversampled arp filter),
48 kHz, 16-sample blocks (0.33 ms).

## Build
Needs `arm-none-eabi-gcc` and [libDaisy](https://github.com/electro-smith/libDaisy) (build it once with `make` in its folder).

    cd TwentyEngineDaisy
    make LIBDAISY_DIR=/path/to/libDaisy

## Flash (the firmware runs from RAM, loaded by the Daisy bootloader)
The firmware (~157 KB) is bigger than the STM32H750's 128 KB internal flash, so it uses the Daisy bootloader.
No toolchain needed: install `dfu-util` (Mac: `brew install dfu-util`; Windows: dfu-util from dfu-util.sourceforge.net
plus the WinUSB driver via Zadig) and use the two files from the release (`dsy_bootloader_v6_4-intdfu-2000ms.bin`,
`TwentyEngineDaisy.bin`), or `make program-boot` / `make program-dfu` after building.

1. **Once, the bootloader:** put the Patch SM in DFU mode (hold BOOT, tap RESET, release BOOT), then

        dfu-util -a 0 -s 0x08000000:leave -D dsy_bootloader_v6_4-intdfu-2000ms.bin -d ,0483:df11

2. **The firmware (and every update):** tap RESET; the LED pulses for 2 s (bootloader waiting) - press BOOT during that
   time to keep it waiting, then

        dfu-util -a 0 -s 0x90040000:leave -D TwentyEngineDaisy.bin -d ,0483:df11

After that the Patch SM starts Twenty Engine by itself at every power-up (after the bootloader's 2 s wait).
The [Daisy Web Programmer](https://flash.daisy.audio) can do both steps in Chrome as well.

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
