// mk_control.h - the Korg microKONTROL as a controller for Twenty Engine (instead of the MS-20ic).
//
// The microKONTROL runs in its Native KORG mode (Korg's microKONTROL MIDI implementation, "Native KORG Mode"):
// every control reports to the host as SysEx, and the host writes the eight small displays, the main display
// and the LEDs. The patch bay of the MS-20ic has no counterpart and is left out.
//
//   8 encoders   the settings of the current page; < and > step through the pages (main display: page name,
//                small displays: the encoders' names; a turned encoder shows its value for a second)
//   8 sliders    performance controls, the same on every page; after a sound recall a slider takes over only
//                once it passes the current value (no jumps)
//   16 pads      the memories: tap = recall, hold 2 s = save (as SWITCH + key on the MS-20ic); with SCENE
//                held, memories 17-32; ENTER = the panel. The pad of the active memory is lit.
//   joystick     X = pitch bend, Y up = modulation wheel
//   pedal        the MS-20ic's momentary SWITCH
//   keyboard     notes, as they are (standard MIDI in native mode)
//
// Engine-neutral: a Sink gives access to the engine (settings as raw Ksoloti values, MIDI into the engine,
// the active memory slot) and to the controller (SysEx out). Main entry points:
//   start(sink)            native mode on, draw everything (call when the controller is found)
//   native(cmd, a, b, sink) a native message from the controller (F0 42 4g 6E 00 cmd a b F7)
//   tick(ms, sink)         about every 20 ms: puts value displays back to names, follows the active memory
//   stop(sink)             native mode off (when closing)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ms20 {

struct MkSink {
  virtual int32_t get(int idx) = 0;                       // a setting of the live engine (raw)
  virtual void set(int idx, int32_t raw) = 0;             // set it (reported as a change of the live engine)
  virtual void midi(uint8_t s, uint8_t d1, uint8_t d2) = 0;   // into the live engine as if from the MS-20ic
  virtual int memSlot() = 0;                              // active memory, -1 = panel
  virtual void sysex(const uint8_t *b, int n) = 0;        // to the microKONTROL
  virtual ~MkSink() {}
};

template <int NP>
class MicroKontrol {
public:
  // ---- the layout ----
  struct Ctl { const char *object, *name, *label; };      // a setting (by patch object + name), 8-character label
  static const int NPAGE = 5;
  struct Page { const char *title; Ctl enc[8]; };
  static const Page *Pages() {
    static const Page p[NPAGE] = {
      {"VCO", {{"MS20_Voices","vco1wave","VCO1WAVE"}, {"MS20_Voices","vco1oct","VCO1 OCT"}, {"MS20_Voices","pw","PW"},
               {"MS20_Voices","vco2wave","VCO2WAVE"}, {"MS20_Voices","vco2oct","VCO2 OCT"}, {"MS20_Voices","vco2semi","VCO2PTCH"},
               {"MS20_Voices","vco2fine","VCO2FINE"}, {"MS20_Voices","portamento","PORTA"}}},
      {"EG", {{"EG1","delay","EG1 DLY"}, {"EG1","attack","EG1 ATT"}, {"EG1","release","EG1 REL"}, {"EG1","amount","EG1 AMT"},
              {"EG2","hold","EG2 HOLD"}, {"EG2","attack","EG2 ATT"}, {"EG2","decay","EG2 DEC"}, {"EG2","release","EG2 REL"}}},
      {"MOD", {{"MG_Morphing_LFO","rate","MG RATE"}, {"MG_Morphing_LFO","shape","MG WAVE"}, {"MG_Morphing_LFO","pitchmod","MG>PITCH"},
               {"MG_Morphing_LFO","lpfmod","MG>LPF"}, {"MG_Morphing_LFO","hpfmod","MG>HPF"}, {"EG2","lpfdepth","EG2>LPF"},
               {"EG2","hpfdepth","EG2>HPF"}, {"MS20_Voices","multitrig","MULTITRG"}}},
      {"ESP", {{"MS20_Voices","arpmode","PLAYMODE"}, {"MS20_Voices","arprate","ARP RATE"}, {"MS20_Voices","arpoct","ARP OCT"},
               {"MS20_Voices","arporder","ARPORDER"}, {"BBD_Delay","time","DLY TIME"}, {"BBD_Delay","feedback","DLY FDBK"},
               {"BBD_Delay","drywet","DLY AMT"}, {"BBD_Delay","bbdfilter","DLY TONE"}}},
      {"OUT", {{"BBD_Delay","wowflutter","WOW"}, {"BBD_Delay","spread","SPREAD"}, {"BBD_Delay","width","WIDTH"},
               {"VCA_Macro","drive","DRIVE"}, {"VCA_Macro","bassboost","BASS"}, {"Filter_Macro","clicknoise","CLICK"},
               {"MS20_Master_Tune","tune","TUNE"}, {"Master_Mixer","extlvl","EXT LVL"}}},
    };
    return p;
  }
  static const Ctl *Sliders() {
    static const Ctl s[8] = {{"Master_Mixer","vco1","VCO1 LVL"}, {"Master_Mixer","vco2","VCO2 LVL"},
                             {"Filter_Macro","hpfcut","HPF CUT"}, {"Filter_Macro","hpfpeak","HPF PEAK"},
                             {"Filter_Macro","lpfcut","LPF CUT"}, {"Filter_Macro","lpfpeak","LPF PEAK"},
                             {"EG2","sustain","EG2 SUS"}, {"VCA_Macro","mastervol","VOLUME"}};
    return s;
  }

  // Lookup of settings: index, kind, range. Filled by the caller from the engine's parameter table.
  struct Info { const char *object, *name, *type; int hmax; };
  explicit MicroKontrol(const Info *info) : inf(info) {
    for (int p = 0; p < NPAGE; p++) for (int e = 0; e < 8; e++) encIdx[p][e] = Find(Pages()[p].enc[e]);
    for (int s = 0; s < 8; s++) { sldIdx[s] = Find(Sliders()[s]); engaged[s] = false; lastSlider[s] = -1; }
    for (int i = 0; i < 9; i++) showUntil[i] = 0;
  }
  bool complete() const {   // every control found in the engine (tests)
    for (int p = 0; p < NPAGE; p++) for (int e = 0; e < 8; e++) if (encIdx[p][e] < 0) return false;
    for (int s = 0; s < 8; s++) if (sldIdx[s] < 0) return false;
    return true;
  }
  int encoderTarget(int page, int e) const { return encIdx[page][e]; }
  int sliderTarget(int s) const { return sldIdx[s]; }
  int page() const { return pg; }

  // ---- messages to the controller ----
  // the controller's global MIDI channel (the "4g" byte): learned from its own messages, 0 until then
  void setGlobal(uint8_t g) { gch = g & 15; gKnown = true; }
  // the controller also sends the joystick as standard pitch bend / modulation (then the native copy is not used)
  void hardware(bool bend, bool mod) { hwBend = bend; hwMod = mod; }
  void Native(MkSink &k, uint8_t cmd, uint8_t a, uint8_t b) const { const uint8_t m[9] = {0xF0, 0x42, (uint8_t)(0x40 | gch), 0x6E, 0x00, cmd, a, b, 0xF7}; k.sysex(m, 9); }
  void Lcd(MkSink &k, int n, int backlight, const char *text) const {   // n 0-7 small displays, 8 main; 8 characters
    uint8_t m[17] = {0xF0, 0x42, (uint8_t)(0x40 | gch), 0x6E, 0x00, 0x22, 0x09, (uint8_t)(((backlight & 7) << 4) | (n & 15))};
    bool end = false;
    for (int i = 0; i < 8; i++) { if (!text[i]) end = true; const char c = end ? ' ' : text[i]; m[8 + i] = (uint8_t)((c < 0x20 || c > 0x7E) ? ' ' : c); }
    m[16] = 0xF7; k.sysex(m, 17);
  }
  void Led(MkSink &k, int n, int mode) const { Native(k, 0x01, (uint8_t)n, (uint8_t)((mode & 3) << 5)); }   // mode 0 off, 1 on, 3 blink

  void start(MkSink &k) {
    // native mode in. Until the controller's global channel is known (it is in every message it sends),
    // the request goes out on all 16: only the matching one is answered (cmd 40, then everything is drawn)
    if (gKnown) Native(k, 0x00, 0x00, 0x01);
    else { const uint8_t keep = gch; for (int c = 0; c < 16; c++) { gch = (uint8_t)c; Native(k, 0x00, 0x00, 0x01); } gch = keep; }
    drawAll(k);
  }
  bool nativeOn() const { return isNative; }
  void stop(MkSink &k) { Native(k, 0x00, 0x00, 0x00); }   // native mode out

  // ---- messages from the controller ----
  void native(uint8_t cmd, uint8_t a, uint8_t b, MkSink &k, uint32_t ms) {
    now = ms;
    switch (cmd) {
      case 0x40: isNative = (b == 0x03); if (isNative) drawAll(k); break;   // native mode is on: draw everything
      case 0x43: encoder(a, b, k); break;
      case 0x44: slider(a, b, k); break;
      case 0x45: pad((a >> 6) & 1, a & 15, k); break;
      case 0x47: k.midi(0xD0, b ? 0x7F : 0x00, 0); break;               // pedal = the MS-20ic's momentary switch
      case 0x48: button(a, b != 0, k); break;
      case 0x4B: joystick(a, b, k); break;
      default: break;
    }
  }

  void tick(MkSink &k, uint32_t ms) {
    now = ms;
    for (int c = 0; c < 8; c++) if (showUntil[c] && (int32_t)(now - showUntil[c]) >= 0) { showUntil[c] = 0; drawColumn(k, c); }
    const int slot = k.memSlot();
    if (slot != litSlot) { padLeds(k, slot); litSlot = slot; }
  }

private:
  const Info *inf;
  int encIdx[NPAGE][8], sldIdx[8], pg = 0, litSlot = -2;
  bool engaged[8]; int lastSlider[8]; int32_t lastSet[8] = {};
  int encAcc[8] = {};                        // switches: steps gather over several detents
  uint32_t showUntil[9], now = 0;
  bool sceneHeld = false; int padDown = -1, padNote = -1; uint8_t gch = 0;
  bool gKnown = false, isNative = false, hwBend = false, hwMod = false;

  int Find(const Ctl &c) const { for (int i = 0; i < NP; i++) if (!strcmp(inf[i].object, c.object) && !strcmp(inf[i].name, c.name)) return i; return -1; }
  enum Kind { UNI, BI, INTV, BOOLV };
  Kind kind(int i) const {
    const char *t = inf[i].type;
    if (!strncmp(t, "frac32.s", 8)) return BI;
    if (!strncmp(t, "frac32", 6)) return UNI;
    if (!strncmp(t, "bool32", 6)) return BOOLV;
    return INTV;
  }
  int maxInt(int i) const { return inf[i].hmax > 0 ? inf[i].hmax - 1 : 13; }
  static const int32_t UNIT = 1 << 21, FULL = 64 << 21;

  void encoder(int n, int d7, MkSink &k) {
    if (n > 7) return;                                                  // main encoder: not used
    const int d = d7 >= 64 ? d7 - 128 : d7;                             // -64..63
    const int i = encIdx[pg][n];
    if (i < 0) return;
    int32_t v = k.get(i);
    switch (kind(i)) {
      case UNI: case BI: {                                              // half a knob unit per detent (128 detents end to end)
        const int64_t lo = kind(i) == BI ? -FULL : 0;
        int64_t nv = (int64_t)v + (int64_t)d * (UNIT / 2);
        if (nv < lo) nv = lo; if (nv > FULL) nv = FULL;
        v = (int32_t)nv; break;
      }
      default: {                                                        // switches: one step per 3 detents
        encAcc[n] += d;
        int steps = encAcc[n] / 3; if (!steps) return;
        encAcc[n] -= steps * 3;
        const int hi = kind(i) == BOOLV ? 1 : maxInt(i);
        int nv = v + steps; if (nv < 0) nv = 0; if (nv > hi) nv = hi;
        if (nv == v) return;
        v = nv; break;
      }
    }
    k.set(i, v);
    showValue(k, n, i, v);
  }

  void slider(int n, int val, MkSink &k) {
    if (n > 7) return;
    const int i = sldIdx[n];
    if (i < 0) return;
    const int64_t lo = kind(i) == BI ? -FULL : 0;
    const int32_t target = (int32_t)(lo + ((int64_t)FULL - lo) * val / 127);
    const int32_t cur = k.get(i);
    if (engaged[n] && cur != lastSet[n]) engaged[n] = false;            // moved by something else (a memory, the DAW)
    if (!engaged[n]) {                                                   // soft takeover: pass the current value first
      const int64_t tol = ((int64_t)FULL - lo) / 40;
      const bool near = (int64_t)target - cur < tol && (int64_t)cur - target < tol;
      bool crossed = false;
      if (lastSlider[n] >= 0) {
        const int32_t prev = (int32_t)(lo + ((int64_t)FULL - lo) * lastSlider[n] / 127);
        crossed = (prev <= cur && target >= cur) || (prev >= cur && target <= cur);
      }
      lastSlider[n] = val;
      if (!near && !crossed) { showPickup(k, n, i, cur); return; }
      engaged[n] = true;
    }
    lastSlider[n] = val;
    lastSet[n] = target;
    k.set(i, target);
    showValue(k, n, i, target, true);
  }

  void pad(bool on, int n, MkSink &k) {
    // memories 1-16 (17-32 with SCENE held): the MS-20ic's SWITCH + key, so the engine's own memory logic decides
    // recall (tap) or save (hold 2 s)
    const int slot = n + (sceneHeld ? 16 : 0);
    if (on) {
      if (padDown >= 0) return;
      padDown = n; padNote = 48 + slot;
      k.midi(0xD0, 0x7F, 0); k.midi(0x90, (uint8_t)padNote, 100);
    } else if (n == padDown) {
      k.midi(0x80, (uint8_t)padNote, 0); k.midi(0xD0, 0x00, 0);
      padDown = -1;
    }
  }

  void button(int n, bool on, MkSink &k) {
    if (n == 5) { sceneHeld = on; Led(k, 0x12, on ? 1 : 0); litSlot = -2; return; }   // SCENE: pads = memories 17-32
    if (!on) return;
    if (n == 0 || n == 1) { pg = (pg + (n == 1 ? 1 : NPAGE - 1)) % NPAGE; for (int c = 0; c < 8; c++) { showUntil[c] = 0; encAcc[c] = 0; } drawAll(k); return; }
    if (n == 2) { k.midi(0xD0, 0x7F, 0); k.midi(0x90, 48 + 36, 100); k.midi(0x80, 48 + 36, 0); k.midi(0xD0, 0x00, 0); }   // ENTER: the panel
  }

  void joystick(int x, int y, MkSink &k) {
    const int bend = x <= 64 ? x * 8192 / 64 : 8192 + (x - 64) * 8191 / 63;   // centre 64 -> 8192 exactly
    if (!hwBend) k.midi(0xE0, (uint8_t)(bend & 127), (uint8_t)(bend >> 7));
    if (!hwMod) k.midi(0xB0, 1, (uint8_t)(y > 64 ? (y - 64) * 127 / 63 : 0));   // up: modulation wheel
  }

  // ---- displays ----
  void drawAll(MkSink &k) {
    char t[9]; snprintf(t, sizeof(t), "%-3s %d/%d", Pages()[pg].title, pg + 1, NPAGE);
    Lcd(k, 8, 2, t);
    for (int c = 0; c < 8; c++) drawColumn(k, c);
    Led(k, 0x18, pg < NPAGE - 1 ? 1 : 0); Led(k, 0x19, pg > 0 ? 1 : 0);   // > and < arrows
    litSlot = -2;
  }
  void drawColumn(MkSink &k, int c) { Lcd(k, c, 2, Pages()[pg].enc[c].label); }
  void showValue(MkSink &k, int c, int i, int32_t v, bool isSlider = false) {
    (void)isSlider;
    char t[12]; Format(i, v, t);
    Lcd(k, c, 3, t);
    showUntil[c] = now + 1000; if (!showUntil[c]) showUntil[c] = 1;
  }
  void showPickup(MkSink &k, int c, int i, int32_t cur) {
    char t[12]; Format(i, cur, t);
    char line[12]; snprintf(line, sizeof(line), "^%s", t);
    Lcd(k, c, 1, line);
    showUntil[c] = now + 1000; if (!showUntil[c]) showUntil[c] = 1;
  }
  void Format(int i, int32_t v, char *out) const {
    const char *n = inf[i].name;
    if (!strcmp(n, "arpmode")) { const double u = v / (double)UNIT; snprintf(out, 9, "%s", u < 18 ? "POLY" : u < 42 ? "ARP" : "LATCH"); return; }
    if (!strcmp(n, "vco1wave")) { static const char *w[] = {"TRI", "SAW", "PULSE", "NOISE"}; snprintf(out, 9, "%s", (v >= 0 && v < 4) ? w[v] : "?"); return; }
    if (!strcmp(n, "vco2wave")) { static const char *w[] = {"SAW", "SQUARE", "PULSE", "RING"}; snprintf(out, 9, "%s", (v >= 0 && v < 4) ? w[v] : "?"); return; }
    if (!strcmp(n, "vco1oct") || !strcmp(n, "vco2oct")) { static const char *o[] = {"32'", "16'", "8'", "4'"}; snprintf(out, 9, "%s", (v >= 0 && v < 4) ? o[v] : "?"); return; }
    switch (kind(i)) {
      case BOOLV: snprintf(out, 9, "%s", v ? "ON" : "OFF"); return;
      case INTV: snprintf(out, 9, "%d", (int)v); return;
      default: snprintf(out, 9, "%.1f", v / (double)UNIT); return;
    }
  }
  void padLeds(MkSink &k, int slot) {
    for (int p = 0; p < 16; p++) {
      const int s = p + (sceneHeld ? 16 : 0);
      Led(k, p, s == slot ? 1 : 0);
    }
  }
};

}  // namespace ms20
