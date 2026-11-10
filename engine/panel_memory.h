// panel_memory.h - the MS-20ic's last panel, kept on the computer.
//
// The MS-20ic sends its whole panel (volume switch, patch jacks, every knob) only when it starts up; there is
// no message that asks for it (checked with USB captures of Korg's own software). So the plugin keeps a copy of
// everything the controller has sent - the last value of every knob, the cables plugged in, the volume switch -
// in MS20PANEL.BIN next to MS20MEM.BIN, and when it connects to the controller it plays that copy into its
// engine as a start-up dump: the sound matches the panel and the start-up light show runs, no power cycling.
//
// Audio thread: onMessage() for every message from the controller, replay() to make the dump.
// Main thread:  load() once, save() when changed() says so (it copies a consistent snapshot first).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <atomic>

namespace ms20 {

struct PanelMemory {
  static const int MAXCABLES = 96;
  struct State {
    int16_t cc[128];               // last value, -1 = never seen
    uint8_t chan;                  // the controller's MIDI channel (0..15)
    int8_t power;                  // volume switch: 1 on, 0 off, -1 unknown
    uint8_t ncab; uint8_t cab[MAXCABLES][2];   // plug-in / connect events (type 4) still in effect, in order
    void clear() { for (int i = 0; i < 128; i++) cc[i] = -1; chan = 0; power = -1; ncab = 0; }
  };

  PanelMemory() { s.clear(); }

  // ---- audio thread ----
  void onMessage(uint8_t st, uint8_t d1, uint8_t d2) {
    if ((st & 0xF0) != 0xB0) return;
    s.chan = st & 0x0F;
    if (d1 == 99) { m99 = d2; h99 = true; h98 = h6 = false; return; }   // patch events: 99 type, 98 jack a, 6 jack b
    if (d1 == 98) { m98 = d2; h98 = true; }
    else if (d1 == 6) { m6 = d2; h6 = true; }
    if (d1 == 98 || d1 == 6) {
      if (h99 && h98 && h6) { h99 = h98 = h6 = false; event(m99, m98, m6); }
      return;
    }
    if (d1 >= 120 || d1 == 102 || d1 == 103) return;    // channel modes; memory keys (Ksoloti / Teensy editions)
    if (s.cc[d1] != d2) { s.cc[d1] = d2; bump(); }
  }
  // the start-up dump: send(status, d1, d2) for each message. Starts with the volume switch off, so switching it
  // on at the end runs the light show (unless the panel was left off).
  template <class F> void replay(F send) const {
    const uint8_t c = 0xB0 | s.chan;
    auto ev = [&](int t, int a, int b) { send(c, 99, (uint8_t)t); send(c, 98, (uint8_t)a); send(c, 6, (uint8_t)b); };
    ev(6, 0, 0);
    for (int j = 0; j < 35; j++) ev(5, j, j);                       // every jack empty (as the controller's own dump)
    for (int k = 0; k < s.ncab; k++) ev(4, s.cab[k][0], s.cab[k][1]);
    for (int i = 0; i < 128; i++) if (s.cc[i] >= 0) send(c, (uint8_t)i, (uint8_t)s.cc[i]);
    ev(6, 0, s.power == 0 ? 0 : 127);
  }
  bool known() const { return seen; }   // a panel was loaded or received

  // ---- main thread ----
  uint32_t changed() const { return ver.load(std::memory_order_acquire); }
  bool snapshot(State &out) const {   // a copy not torn by the audio thread
    for (int tries = 0; tries < 8; tries++) {
      const uint32_t a = ver.load(std::memory_order_acquire);
      memcpy(&out, &s, sizeof(State));
      std::atomic_thread_fence(std::memory_order_acquire);
      if (ver.load(std::memory_order_acquire) == a) return true;
    }
    return false;
  }
  bool save(const char *path) const {
    State c; if (!snapshot(c)) return false;
    char tmp[1100]; snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb"); if (!f) return false;
    const uint8_t hdr[6] = {'M', 'S', 'P', 'N', 1, (uint8_t)sizeof(State)};
    bool ok = fwrite(hdr, 1, 6, f) == 6 && fwrite(&c, sizeof(State), 1, f) == 1;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return false; }
    remove(path);                       // Windows: rename does not replace
    return rename(tmp, path) == 0;
  }
  // before audio starts
  bool load(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return false;
    uint8_t hdr[6]; State c;
    const bool ok = fread(hdr, 1, 6, f) == 6 && !memcmp(hdr, "MSPN", 4) && hdr[4] == 1 && hdr[5] == (uint8_t)sizeof(State) &&
                    fread(&c, sizeof(State), 1, f) == 1 && c.ncab <= MAXCABLES;
    fclose(f);
    if (!ok) return false;
    s = c; seen = true; return true;
  }

private:
  State s;
  std::atomic<uint32_t> ver{0};
  bool seen = false;
  int m99 = 0, m98 = 0, m6 = 0; bool h99 = false, h98 = false, h6 = false;
  void bump() { seen = true; ver.fetch_add(1, std::memory_order_release); }
  void del(int a, int b) {
    int w = 0;
    for (int k = 0; k < s.ncab; k++) { const bool hit = (s.cab[k][0] == a && s.cab[k][1] == b) || (s.cab[k][0] == b && s.cab[k][1] == a);
      if (!hit) { s.cab[w][0] = s.cab[k][0]; s.cab[w][1] = s.cab[k][1]; w++; } }
    s.ncab = (uint8_t)w;
  }
  void event(int t, int a, int b) {
    if (t == 6) { const int8_t p = (a != b) ? 1 : 0; if (p != s.power) { s.power = p; bump(); } return; }
    if ((t != 4 && t != 5) || a < 0 || a > 34 || b < 0 || b > 34) return;
    del(a, b);
    if (t == 4 && s.ncab < MAXCABLES) { s.cab[s.ncab][0] = (uint8_t)a; s.cab[s.ncab][1] = (uint8_t)b; s.ncab++; }
    bump();
  }
};

}  // namespace ms20
