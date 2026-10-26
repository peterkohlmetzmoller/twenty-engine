# TwentyEngine (working name)

An AU instrument for macOS that runs the MS-20 iC Ksoloti patch on a computer.
No custom graphics: Logic shows the parameters in its Controls view.

## How it is put together
- `patch/`    the Ksoloti patch the engine is generated from
- `engine/`   `gen_engine.py` turns the patch into `engine_gen.h` (the patch's own code, wiring, execution
              order, parameter defaults and CC assignments); `ms20_host.h` runs it in a host
              (48 kHz engine + resampler, MIDI timing, parameters both ways, LED queue)
- `TwentyEngine.cpp/.h`  the iPlug2 plugin around the engine
- `led_port.h`           sends the LED SysEx straight to the MS-20 iC through Core MIDI
- `tests/`               checks that run on any computer
- `.github/workflows/build-au.yml`  GitHub builds, tests, signs (ad hoc for now) and validates the AU

## New patch version
Replace the file in `patch/`, update the file name in the workflow, push. GitHub regenerates the engine and builds.

## Names
The product name, manufacturer and IDs are placeholders: `config.h` and `resources/TwentyEngine-AU-Info.plist`.
