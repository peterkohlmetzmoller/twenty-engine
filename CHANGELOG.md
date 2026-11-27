# Twenty Engine changelog

## 1.1.1 (in progress)

### New
- Teensy: DIN MIDI in on pin 0 (Serial1 RX) through an optocoupler. Another keyboard or a sequencer plays the
  engine without a computer. Set DIN_MIDI_IN to false at the top of the sketch to use pin 0 for a button.

### Changed
- Snappier envelopes: with ATTACK at the fast end the attack takes about 2 ms (was about 8 ms), and DECAY / RELEASE
  at the fast end die away in about 4 ms (was about 17 ms). Only the fast quarter of each knob changes; from the
  middle up the times are as before. EG 1 and EG 2, every edition.

## 1.1

### New
- Two plugins in one installer:
  - Twenty Engine UI: the whole MS-20 panel on screen, with knobs, patch cables and LEDs. Works with or without an
    MS-20ic: MS-20ic mode while the iC is connected (regions keep their sound), a standard synth plugin otherwise.
    The mode is in the status bar.
  - Twenty Engine MS-20ic: the classic version without a window, played from the iC, with each region keeping its
    sound. Projects made with Twenty Engine 1.0 open with it.
- Standalone app "Twenty Engine" for Mac, Windows and Linux: the panel window, audio and MIDI settings, and every
  take saved as a 24-bit WAV in Music/Twenty Engine/Captures.
- Free web edition: Twenty Engine in Chrome and Edge, played from the MS-20ic (with its LEDs) or any MIDI keyboard
  over Web MIDI, at peterkohlmetzmoller.github.io/twenty-engine. Sounds stay in the browser. No takes.
- Presets the normal way: the DAW's preset menu lists the panel and the 36 sounds, and a saved preset keeps the
  whole sound, cables included. Ableton Live on the Mac gets them as preset files.
- CC learn in the window (UI plugin, app, web): right-click a knob, move a controller. One map for your setup.
- Sounds remember their cables: a stored sound brings back its patch until you touch a cable. Switch + top C held
  2 s: the recalled sound plays with the cables plugged in.
- The patch panel's VCA: VCA OUT = VCA IN x VCA CTRL IN. VCA CTRL IN stays the main volume until VCA OUT is patched.
- INITIAL GAIN: a cable keeps the last note sounding at the source's level.
- VCO1+2 CV IN takes any source as a pitch input for both VCOs (KBD CV OUT there is still mono).
- MG TRI OUT follows the WAVEFORM knob, like the original MS-20.
- Wheel: no vibrato while a cable is in WHEEL OUT, so the wheel is a pure modulation source. The wheel also
  responds faster.
- Retrigger: in mono every new key can start the envelopes again (RETRIG in the window, or a Teensy button).
- ESP knobs pick up after a layer change: the delay time no longer jumps to where the arp rate was.
- The TRIG LED flashes with the delay repeats while DELAY AMOUNT is up.
- Arp sync: in the window the arp can follow the DAW's tempo, 1/4 to 1/32 with triplets.
- Bug log: LOG in the window (Copy log in Twenty Engine MS-20ic's window) copies what happened lately with your
  setup; the blue address opens an email to twentyenginesupport@gmail.com.
- Teensy: any function on any button (function switch, next / previous sound, panel, mono, ESP layer, tap tempo,
  retrigger), and the KEY_CLICK setting.
- MIDI: CC 104 function switch, CC 105 ESP layer latch, CC 106 tap.

### Fixes
- Volume off/on always reads the knobs and plays the light show, also in Ableton Live and other VST3 hosts on Windows.
- Turning the volume all the way down counts as off, even if the DAW drops the iC's power message.
- Mono + arp: VCO 1's phaser saw was a plain saw.
- Delay: the wet signal is louder at full DELAY AMOUNT.
- Arp: from 1 o'clock up the RATE knob holds the top speed (about 20 steps a second); faster steps outran the
  envelopes.
- Key Click is softer by default (3, was 6).

### Manual
- New in 1.1, first patches drawn on the panel, the panel window, the app and the web edition.
- Key Click explained, and how to turn it off.
- Windows: remove KORG's USB-MIDI driver; match the DAW's sample rate to your audio device (Bluetooth headphones
  are often 44.1 kHz only).
- Teensy guide: the buttons.

## 1.0.3
- Honest connection status: "Connected" now means the plugin also reaches the iC's LEDs.
- Light show on connect, so you see the plugin's messages get through.
- Windows: a reversed SWITCH (some repaired units) is detected at power-on and flipped.
- Manual and Windows guide: remove KORG's USB-MIDI driver on Windows.

## 1.0.2 (Windows)
- Takes keep their sound through a loopMIDI port named "Twenty Engine Panel". Outside a take, the engine goes back
  to the panel's sound.

## 1.0.1 (Windows)
- The plugin opens the MS-20ic's MIDI input itself only where Windows lets programs share a port (Windows MIDI
  Services). On Windows 10, REAPER no longer loses the controller.

## 1.0
- First release: polyphonic MS-20 engine played from a stock MS-20ic. AU and VST3 for Mac, VST3 for Windows, plus
  Ksoloti and Teensy builds that go inside the iC.
