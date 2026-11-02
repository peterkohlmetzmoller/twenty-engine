# Twenty Engine

An 8-voice polysynth engine for the Korg MS-20 iC, by Peter Kohlmetz Moller.
This repository builds the plugin edition (AU and VST3 for macOS, VST3 for Windows; AAX optional) that runs
the same engine as the Ksoloti patch, played from a stock MS-20 iC. No graphical interface, on purpose:
the iC is the interface.

Free software under the GNU GPL v3 (`LICENSE`), with an additional permission for the Avid AAX SDK
(`LICENSE-EXCEPTIONS.md`).

## How it is put together
- `patch/`    the Ksoloti patch the engine is generated from
- `engine/`   `gen_engine.py` turns the patch into `engine_gen.h`; `ms20_host.h` runs it in a host
              (48 kHz engine + resampler, MIDI timing, parameters both ways, LED queue, patch memory file)
- `TwentyEngine.cpp/.h`  the iPlug2 plugin around the engine
- `led_port.h`           LED SysEx to the iC and controller detection (Core MIDI on Mac, WinMM on Windows)
- `installer/`           Mac .pkg and Windows .exe installer definitions
- `tests/`               checks that run on any computer
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
Product "Twenty Engine", manufacturer "Peter Kohlmetz Moller": `config.h` and `resources/*-Info.plist`.
