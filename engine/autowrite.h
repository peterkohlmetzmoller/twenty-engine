// autowrite.h - Twenty Engine Automation: what goes into the DAW's automation, and when.
//
// Two engines, as in the Instrument plugin:
//   live engine      - the panel: played straight from the controller, shows on the LEDs. The DAW never changes it.
//   playback engine  - plays the track: the regions' notes with the values of the DAW's automation lanes.
//
// The rules:
//   1. Nothing is written while the DAW is stopped or only playing. Knob moves then change the live engine
//      (what you hear when you play), never the automation, so listening back can never change a take.
//   2. When recording starts, every parameter where the panel differs from what the automation holds at that
//      spot is written once: the take starts with the sound you hear. Nothing else is touched, so recording
//      over a take whose sound you still have only adds the knobs you move.
//   3. While recording, every knob move is written.
// A host that does not say whether it records: knob moves are written whenever it runs (as before), and nothing
// is written at the start (it cannot tell a take from a playback, and writing the panel would replace takes).
#pragma once
#include <stdint.h>

namespace ms20 {

template <int N>
struct AutoWrite {
  int32_t panel[N] = {};     // the sound on the controller (raw engine units)
  bool frac[N] = {};         // knob-type parameters (compared with a tolerance)
  bool panelMoved = false;   // the controller has changed something since the plugin opened
  static const int32_t FRAC_TOL = 41943;   // 0.02 of a knob unit (1<<21 raw per unit): rounding through the host

  bool differs(int i, int32_t a, int32_t b) const {
    int64_t d = (int64_t)a - (int64_t)b; if (d < 0) d = -d;
    return frac[i] ? d > FRAC_TOL : d != 0;
  }
  // the project's saved sound (state load, first reset): the panel starts there until the controller moves something.
  // Returns true if the panel took the value (the caller then sets the live engine too).
  bool fromProject(int i, int32_t raw) { if (panelMoved) return false; panel[i] = raw; return true; }
  // a change made on the live engine (knob, memory recall, ESP layer, patch cable). True: write it to the host now.
  bool liveChange(int i, int32_t raw, bool recording) { panel[i] = raw; panelMoved = true; return recording; }
  // recording starts: host[] holds what the automation plays at that spot. write(i, raw) for each difference.
  template <class F> int recordStart(const int32_t *host, F write) const {
    int n = 0;
    for (int i = 0; i < N; i++) if (differs(i, panel[i], host[i])) { write(i, panel[i]); n++; }
    return n;
  }
};

// knob moves are written now?
inline bool hostWrites(bool running, bool known, bool recording) { return running && (known ? recording : true); }
// a take starts (write the panel's differences)? Only when the host says it records.
inline bool hostRecording(bool running, bool known, bool recording) { return running && known && recording; }

}  // namespace ms20
