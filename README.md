# Twenty Engine

An 8-voice polysynth engine for the Korg MS-20ic, by Peter Kohlmetz Moller. One engine, generated from a Ksoloti
patch, in every edition:

- **Twenty Engine UI** (AU, VST3; Mac and Windows): the whole MS-20 panel in a window. With an MS-20ic it follows
  the iC and, in Logic, keeps each region's sound; without one it is a normal synth plugin (automation, presets,
  CC learn).
- **Twenty Engine MS-20ic** (AU, VST3): no window, the MS-20ic is the interface. Keeps the 1.0 plugin IDs, so
  projects made with "Twenty Engine" 1.0 open with it.
- **Twenty Engine**, the standalone app (Mac, Windows, Linux): the panel window, audio and MIDI settings, and every
  take saved as a WAV.
- **The free web edition** (`web/`): the panel and the engine in the browser (WebAssembly, Web MIDI). Play it at
  https://peterkohlmetzmoller.github.io/twenty-engine/ (the `gh-pages` branch, updated by CI from `main`).
- **Ksoloti Core** and **Teensy 4.1** (inside the MS-20ic), and the Daisy Patch SM (beta).

Free software under the GNU GPL v3 (`LICENSE`), with an additional permission for the Avid AAX SDK
(`LICENSE-EXCEPTIONS.md`). The owner's manual is built from `manual/manual.py`.

## How it is put together
- `patch/`    the Ksoloti patch the engine is generated from (`MS20_monopoly_v51.axp` for 1.1, named in the
              workflow's `PATCH:`). The 1.1 changes are scripted from v50c, in this order: `tools/v51_edits.py`,
              `v51_cables.py`, `v51_drone.py`, `v51_vca.py`, `v51_musts.py` (each: input .axp, output .axp)
- `engine/`   `gen_engine.py` turns the patch into `engine_gen.h`; `ms20_host.h` runs it in a host (48 kHz engine +
              resampler, MIDI timing, parameters both ways, LED queue, patch memory file); `state_format.h`
              (saved state), `cc_map.h` (CC learn), `capture.h` (the apps' takes)
- `TwentyEngine.cpp/.h`  the iPlug2 plugin and app around the engine; `tools/make_ui_variant.py` generates the
              Twenty Engine UI source (own name and IDs, and the app "Twenty Engine")
- `ui/`       the panel window (`StudioUI.h`), shared by the plugin, the apps and the web page
- `led_port.h`           the MS-20ic's own MIDI port: LEDs, controller detection (Core MIDI, WinMM, ALSA)
- `linux/`    the Linux app (DPF, with a small IGraphics stand-in on NanoVG)
- `web/`      the web edition: `build.sh` builds it with Emscripten
- `teensy/`, `daisy/`    the Teensy 4.1 and Daisy editions
- `installer/`           Mac .pkg and Windows .exe installer definitions
- `manual/`   the owner's manual, the Teensy guide and the Windows quick guide (reportlab)
- `tests/`    checks that run on any computer (engine, saved state, cables, CC learn, takes, the web page in Chrome)
- `tests/daw/`           REAPER test: CI installs REAPER on Mac and Windows, plays the plugins (AU and VST3),
                         checks the renders, MIDI CC, automation and saved state. CI also runs pluginval on every plugin.
- `.github/workflows/build.yml`  builds, tests and validates every edition and both installers

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
Products "Twenty Engine UI", "Twenty Engine MS-20ic" and the app "Twenty Engine", manufacturer "Peter Kohlmetz
Moller": `config.h`, `resources/*-Info.plist` and `tools/make_ui_variant.py`.
