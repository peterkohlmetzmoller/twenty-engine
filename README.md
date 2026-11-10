# Twenty Engine

An 8-voice polysynth engine for the Korg MS-20ic, by Peter Kohlmetz Moller.
This repository builds the plugin edition (AU and VST3 for macOS, VST3 for Windows; AAX optional) that runs
the same engine as the Ksoloti patch, as two plugins installed together, played from a stock MS-20ic. No graphical interface, on purpose:
the MS-20ic is the interface.

Free software under the GNU GPL v3 (`LICENSE`), with an additional permission for the Avid AAX SDK
(`LICENSE-EXCEPTIONS.md`).

## Two plugins
- **Twenty Engine Instrument** (`Tw20`). Parameters are not automatable. It records every knob move of a
  take itself (`engine/recall.h`) and plays the take back exactly on a second engine, while the live engine keeps
  following the panel. It needs the controller opened directly next to the DAW: Core MIDI on Mac, WinMM on Windows
  (multi-client with Windows MIDI Services on Windows 11). Without that it runs as a single-engine MIDI instrument.
- **Twenty Engine Automation** (`Tw2S`): the same code built with `TWENTY_STUDIO 1`. Every parameter is automatable,
  and at the first note of a new take it touches all parameters so Latch writes the starting sound.
  `tools/make_variant.py studio` generates its source tree (own name, AU/VST3/AAX IDs, bundle IDs and class prefix).

## How it is put together
- `patch/`    the Ksoloti patch the engine is generated from
- `engine/`   `gen_engine.py` turns the patch into `engine_gen.h`; `ms20_host.h` runs it in a host
              (48 kHz engine + resampler, MIDI timing, parameters both ways, LED queue, patch memory file)
- `TwentyEngine.cpp/.h`  the iPlug2 plugin around the engine
- `led_port.h`           LED SysEx to the MS-20ic and controller detection (Core MIDI on Mac, WinMM on Windows)
- `installer/`           Mac .pkg and Windows .exe installer definitions
- `tools/make_variant.py` generates the Twenty Engine Automation source
- `tests/`               checks that run on any computer (`recall_test.cpp` simulates recording and playback with
                         the timing of Logic and of hosts that report the song position smoothly)
- `tests/daw/`           REAPER test: CI installs REAPER on Mac and Windows, plays both plugins (AU and VST3),
                         checks the renders, MIDI CC, automation and saved state. CI also runs pluginval on every plugin.
- `.github/workflows/build.yml`  builds, tests and validates every format and both installers

## New patch version
Replace the file in `patch/`, update `PATCH:` in the workflow, push. GitHub regenerates the engine and builds.

## Building AAX (Pro Tools)
AAX needs the Avid AAX SDK (free for registered Avid developers, not redistributable) and, for retail Pro Tools,
PACE signing. Neither is part of this repository.

**Locally:** clone this repo, clone iPlug2 at the commit named in the workflow into `iPlug2/`, fetch the VST3 SDK
as the workflow does, and unpack the AAX SDK so that `iPlug2/Dependencies/IPlug/AAX_SDK/CMakeLists.txt` exists.
Then:

    python3 engine/gen_engine.py patch/<current patch>.axp engine/engine_gen.h
    cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"      # Windows: -G "Visual Studio 17 2022" -A x64 -T ClangCL
    cmake --build build --config Release --target TwentyEngine-aax

The unsigned `.aaxplugin` loads in Pro Tools Developer. For retail Pro Tools, sign it with your own PACE tools
(wraptool or AAX Cloud Signing).

**On GitHub:** pack the SDK folder as `AAX_SDK` in a `.tar.xz`, base64-encode it, split it into parts and store them as
the secrets `AAX_SDK_B64_1` … `AAX_SDK_B64_7`. The workflow then also builds unsigned AAX for Mac and Windows.

## Names
Products "Twenty Engine Instrument" and "Twenty Engine Automation", manufacturer "Peter Kohlmetz Moller": `config.h`,
`resources/*-Info.plist` and `tools/make_variant.py`.
