# Twenty Engine for Linux (beta)

The standalone app: the MS-20 panel window, played from the Korg MS-20ic (knobs, cables, switch and LEDs)
or from any MIDI keyboard.

## Run it

    tar xzf TwentyEngine-Linux-x86_64.tar.gz     (or ...-arm64 on ARM: Raspberry Pi 4/5, Apple Silicon VMs)
    cd TwentyEngine-Linux-x86_64
    ./TwentyEngine

- **Sound:** if JACK (or PipeWire's JACK) is running, the app is a JACK client: connect its outputs as you like.
  Without JACK it plays through ALSA/PulseAudio on its own.
- **MS-20ic:** plug it in (before or after starting). The app finds it by name and connects to it itself; the
  status bar says "Connected · MS-20ic". Switch the volume knob off and on once so the app reads the knobs.
- **Other keyboards** (USB MIDI) are connected automatically and play the keys.
- **Files:** the 36 sound memories and the last panel are kept in `~/.config/twenty-engine/`.

Libraries it needs (normally already there on a desktop): ALSA (`libasound2`), OpenGL, X11.

## In a virtual machine

- VMware: give the VM the MS-20ic (VM > USB > connect "KORG MS-20 Controller"), and choose a sound card for the VM.
- An Apple Silicon Mac runs ARM Linux in VMware Fusion: use the **arm64** download there.
- Expect more audio latency than on a real machine; it is fine for testing.

## Source

GPL v3. Build from the source: see `linux/CMakeLists.txt`.
