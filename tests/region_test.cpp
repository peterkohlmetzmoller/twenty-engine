// region_test.cpp - Twenty Engine Automation: the sound carried in the region (engine/paramstream.h),
// with the real engines, against a simulated Logic.
// Build: clang++ -std=c++17 -O2 -w -o region_test region_test.cpp && ./region_test
//
// Logic model (from Pete's tests, 2026-11-11):
//  - a recording track with input "All" records the controller AND the plugin's own port; with count-in the
//    recording (and the region) starts at the count-in;
//  - while the transport runs, the track's live input also reaches the plugin a little later (echo);
//  - Logic resets the plugin at every start and does NOT resend the region's controllers when playback starts
//    inside a region (no chase; seen in the plugin's log, 2026-11-11). The plugin keeps the region's
//    snapshots by song position (StreamMemory) and sets the playback engine from them at a start or jump.
// The plugin is wired as in TwentyEngine.cpp (TWENTY_STUDIO): live engine = controller; the panel goes out
// through the port (whole panel at transport start, then every change); the track's stream sets the playback
// engine; the MS-20ic's channel-1 knob CCs on the track are ignored.
#define MS20HOST_TESTING
#define PARAMSTREAM_SIM_CLOCK
#include "../engine/ms20_host.h"
#include "../engine/panel_memory.h"
#include "../engine/paramstream.h"
#include <stdio.h>
#include <vector>
#include <map>
#include <algorithm>
#include <random>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

static const int SR = 48000, BLK = 256, IDLE = 4;     // ~21 ms between idle rounds
static const int THRU = 3;                            // blocks before the track's live input reaches the plugin
struct Ev { int64_t t; uint8_t s, d1, d2; };         // t in samples

struct Sim {
  // ---- Logic ----
  bool running = false, recording = false; int64_t pos = 0;
  std::vector<Ev> region;                             // everything recorded (one track)
  std::vector<Ev> thru;                               // live input on its way back to the plugin
  std::vector<Ev> toPlugin;                           // region playback + chase, due at their time
  std::vector<Ev> controller;                         // what the MS-20ic sends this block
  // ---- plugin ----
  MS20Host live, play; PanelMemory pm; ParamStream<NPARAMS> ps;
  bool wasRunning = false, snapReq = false, mono = false, tap = false; int blocks = 0; int64_t lastSnap = 0;
  int sent = 0, applied = 0;
  StreamMemory<NPARAMS> mem; bool starting = false; int pend = 0; double pendPos = 0;
  bool recordPort = true;                             // false: the track does not record the plugin's port
  bool streamEver = false, following = false; int64_t clk = 0, lastStream = -1000000000; double aheadPos = -1e9; int32_t seen[NPARAMS] = {};
  std::map<int64_t, std::vector<int32_t>> liveAt;     // live settings at each idle while recording (by song position)
  Sim() {
    live.reset(SR); play.reset(SR);
    for (int i = 0; i < NPARAMS; i++) {
      const char *ty = kParamInfo[i].type;
      ps.kind[i] = !strncmp(ty, "frac32.s", 8) ? ParamStream<NPARAMS>::BI : !strncmp(ty, "frac32", 6) ? ParamStream<NPARAMS>::UNI : ParamStream<NPARAMS>::INTV;
    }
    // the playback engine starts on a different sound (as after loading a project)
    for (int i = 0; i < NPARAMS; i++) play.setParamQuiet(i, live.getParamRaw(i) ^ 0x300000);
  }
  void portSend(uint8_t s, uint8_t d1, uint8_t d2) {
    sent++;
    if (recording && recordPort) region.push_back({pos, s, d1, d2});
    if (running && recordPort) thru.push_back({pos + THRU * BLK, s, d1, d2});
  }
  void applyStream(int slot, int32_t raw) {
    applied++;
    if (slot < NPARAMS) { play.setParamQuiet(slot, raw); return; }
    const bool on = raw != 0;
    const int a = slot == ParamStream<NPARAMS>::SLOT_MONO ? 31 : 33, b = slot == ParamStream<NPARAMS>::SLOT_MONO ? 25 : 3;
    play.midi(0, 0xB0, 99, on ? 4 : 5); play.midi(0, 0xB0, 98, (uint8_t)a); play.midi(0, 0xB0, 6, (uint8_t)b);
  }
  void trackMsg(const Ev &e) {
    if (ps.push(e.s, e.d1, e.d2)) return;
    const uint8_t ty = e.s & 0xF0;
    if (ty == 0xB0 && e.d1 != 1) return;                       // the MS-20ic's knob CCs on the track: ignored
    play.midi(0, e.s, e.d1, e.d2);
  }
  void cableFlags() {
    PanelMemory::State st; pm.snapshot(st);
    bool m = false, t = false;
    for (int k = 0; k < st.ncab; k++) {
      const int a = st.cab[k][0], b = st.cab[k][1];
      if ((a == 31 && b == 25) || (a == 25 && b == 31)) m = true;
      if ((a == 33 && b == 3) || (a == 3 && b == 33)) t = true;
    }
    if (m != mono) { mono = m; ps.encode(ParamStream<NPARAMS>::SLOT_MONO, m, [&](uint8_t s, uint8_t a, uint8_t b) { portSend(s, a, b); }); }
    if (t != tap) { tap = t; ps.encode(ParamStream<NPARAMS>::SLOT_TAP, t, [&](uint8_t s, uint8_t a, uint8_t b) { portSend(s, a, b); }); }
  }
  void idle() {
    auto send = [&](uint8_t s, uint8_t a, uint8_t b) { portSend(s, a, b); };
    const int64_t now = ParamStream<NPARAMS>::NowMs();
    if (snapReq || (running && now - lastSnap >= ParamStream<NPARAMS>::SNAP_EVERY_MS)) {   // the whole panel at transport start, then every second
      snapReq = false; lastSnap = now;
      ps.encodeMark(send);
      for (int i = 0; i < NPARAMS; i++) ps.encode(i, live.getParamRaw(i), send);
      ps.encode(ParamStream<NPARAMS>::SLOT_MONO, mono, send); ps.encode(ParamStream<NPARAMS>::SLOT_TAP, tap, send);
    }
    ParamSet p; while (live.popEngineChange(p)) ps.encode(p.idx, p.raw, send);   // every change
    cableFlags();
    { ParamSet q; while (play.popEngineChange(q)) {} }
    if (recording) { std::vector<int32_t> v(NPARAMS); for (int i = 0; i < NPARAMS; i++) v[i] = live.getParamRaw(i); liveAt[pos] = v; }
  }
  void block() {
    ParamStream<NPARAMS>::SimMs() += (BLK * 1000) / SR;
    // the track: region playback / chase and echoes due in this block
    std::vector<Ev> due;
    for (auto it = toPlugin.begin(); it != toPlugin.end();) { if (it->t < pos + BLK) { due.push_back(*it); it = toPlugin.erase(it); } else ++it; }
    for (auto it = thru.begin(); it != thru.end();) { if (it->t < pos + BLK) { due.push_back(*it); it = thru.erase(it); } else ++it; }
    std::stable_sort(due.begin(), due.end(), [](const Ev &a, const Ev &b) { return a.t < b.t; });
    for (auto &e : due) trackMsg(e);
    const double songPos = (double)pos / SR;
    // as the plugin: at a start, the remembered sound for this song position first
    if (running && starting) { mem.breakRun(); if (const int32_t *v = mem.at(songPos, 0.125)) { for (int i = 0; i < NPARAMS; i++) play.setParamQuiet(i, v[i]); lastStream = clk; } }
    starting = false;
    // as the plugin: no region data for a while (or never): follow the panel; a remembered snapshot just ahead wins
    if (running && streamEver && clk - lastStream > SR / 2)
      if (auto *a = mem.within(songPos, songPos + 0.125)) if (a->pos != aheadPos) { for (int i = 0; i < NPARAMS; i++) play.setParamQuiet(i, a->v[i]); aheadPos = a->pos; lastStream = clk; }
    const bool follow = !streamEver || (running && clk - lastStream > SR * 3 / 2);
    if (follow) for (int i = 0; i < NPARAMS; i++) { const int32_t v = live.getParamRaw(i); if (!following || v != seen[i]) { play.setParamQuiet(i, v); seen[i] = v; } }
    following = follow;
    ps.flush([&](int slot, int32_t raw) { applyStream(slot, raw); });
    if (ps.lastApplied > 0) { streamEver = true; lastStream = clk; }
    // a region's snapshot: remember it with its song position (its tail may come in the next blocks)
    if (running && ps.lastForeign) { pend = 3; pendPos = songPos; }
    if (running && pend > 0) { if (ps.lastN > 0) { int32_t v[NPARAMS]; for (int i = 0; i < NPARAMS; i++) v[i] = play.getParamRaw(i); mem.store(pendPos, v); } pend--; }
    // the controller, straight to the plugin (and recorded by Logic)
    for (auto &e : controller) { pm.onMessage(e.s, e.d1, e.d2); live.midi(0, e.s, e.d1, e.d2); if (recording) region.push_back({pos, e.s, e.d1, e.d2}); }
    controller.clear();
    if (running && !wasRunning) snapReq = true;
    wasRunning = running;
    if (running) ps.runningBlock(); else ps.stoppedBlock();
    float L[BLK], R[BLK]; live.process(L, R, BLK); play.process(L, R, BLK);
    if (++blocks % IDLE == 0) idle();
    if (running) pos += BLK;
    clk += BLK;
  }
  void run(double secs) { for (int k = 0; k < (int)(secs * SR / BLK); k++) block(); }
  void knob(uint8_t cc, uint8_t v) { controller.push_back({0, 0xB0, cc, v}); }
  void cable(int t, int a, int b) { knob(99, (uint8_t)t); knob(98, (uint8_t)a); knob(6, (uint8_t)b); }
  void note(uint8_t k, bool on) { controller.push_back({0, (uint8_t)(on ? 0x90 : 0x80), k, (uint8_t)(on ? 100 : 0)}); }
  void startRecord(double at) { pos = (int64_t)(at * SR); running = recording = true; ps.resetDecoder(); }
  void startPlay(double at, unsigned seed) {
    pos = (int64_t)(at * SR); running = true; recording = false;
    std::sort(region.begin(), region.end(), [](const Ev &a, const Ev &b) { return a.t < b.t; });
    play.reset(SR);                                             // Logic resets the plugin at every start
    toPlugin.clear();                                           // and does not chase the region's controllers
    for (auto &e : region) if (e.t >= pos) toPlugin.push_back(e);
    starting = true; (void)seed;
  }
  // a jump while playing (playhead moved): no reset, no chase
  void jump(double at) { pos = (int64_t)(at * SR); toPlugin.clear(); for (auto &e : region) if (e.t >= pos) toPlugin.push_back(e); starting = true; }
  void stop() { running = recording = false; toPlugin.clear(); mem.breakRun(); run(0.2); }
  std::vector<int32_t> liveAtSong(double t) { auto it = liveAt.upper_bound((int64_t)(t * SR)); --it; return it->second; }
  int compare(const std::vector<int32_t> &want, const char *what, bool print = true) {
    int bad = 0;
    for (int i = 0; i < NPARAMS; i++) {
      const int32_t a = want[i], b = play.getParamRaw(i);
      const bool frac = !strncmp(kParamInfo[i].type, "frac32", 6);
      const int64_t d = (int64_t)a - b;
      if (frac ? (d > 16500 || d < -16500) : d != 0) { if (print && bad < 4) printf("      %s: param %d (%s) want %d got %d\n", what, i, kParamInfo[i].label, a, b); bad++; }
    }
    return bad;
  }
};

// a sound: a set of the MS-20ic's knob CCs (taken from its own start-up dump)
static const uint8_t kKnobs[] = {0x4A, 0x1C, 0x0B, 0x18, 0x05, 0x1E, 0x4C, 0x59, 0x46, 0x14, 0x1B, 0x5A, 0x4B, 0x5B, 0x12, 0x1D, 0x4F, 0x58, 0x47};
static void dial(Sim &S, int seed) { for (size_t i = 0; i < sizeof(kKnobs); i++) S.knob(kKnobs[i], (uint8_t)((seed * 37 + i * 23) % 120 + 4)); S.run(0.1); }

int main() {
  Sim S;
  S.run(0.2);
  // ---- take 1: sound A, count-in (the transport starts 2 s before the notes), a sweep on CC 0x4A
  dial(S, 1);
  std::vector<int32_t> soundA(NPARAMS); for (int i = 0; i < NPARAMS; i++) soundA[i] = S.live.getParamRaw(i);
  S.startRecord(0.0); S.run(2.0);
  S.note(60, true); S.run(1.0);
  for (int k = 0; k < 20; k++) { S.knob(0x4A, (uint8_t)(20 + k * 5)); S.run(0.05); }
  S.run(1.0); S.note(60, false); S.run(0.5);
  S.stop();
  // ---- a new sound B, dialled while stopped, then take 2 without count-in, with the mono cable patched halfway
  dial(S, 7);
  std::vector<int32_t> soundB(NPARAMS); for (int i = 0; i < NPARAMS; i++) soundB[i] = S.live.getParamRaw(i);
  { int d = 0; for (int i = 0; i < NPARAMS; i++) if (soundA[i] != soundB[i]) d++; printf("      sound A and B differ in %d settings\n", d);
    check(d >= 10, "sound B differs from sound A (the test is meaningful)"); }
  S.startRecord(10.0); S.run(0.3); S.note(64, true); S.run(1.0); S.cable(4, 31, 25); S.run(1.5); S.note(64, false); S.run(0.5);
  S.stop();
  printf("      recorded %zu events, plugin sent %d stream messages\n", S.region.size(), S.sent);
  int portEv = 0; for (auto &e : S.region) if ((e.s & 0x0F) >= 14) portEv++;
  check(portEv > 2 * NPARAMS, "the region holds the plugin's stream (whole panel + changes)");

  // ---- playback from the start: take 1 must sound like A (with its sweep), not like the panel (B)
  S.applied = 0;
  S.startPlay(0.0, 1); S.run(2.5);
  check(S.compare(S.liveAtSong(2.4), "take 1 before the sweep") == 0, "take 1 plays with sound A (not the panel's B)");
  S.run(2.0);
  check(S.compare(S.liveAtSong(4.4), "take 1 after the sweep") == 0, "take 1 plays its knob sweep");
  check(S.applied > 0, "the playback engine took its settings from the region");
  int liveB = 0; for (int i = 0; i < NPARAMS; i++) if (S.live.getParamRaw(i) != soundB[i] && strncmp(kParamInfo[i].label, "Patch", 5)) liveB++;
  check(liveB == 0, "meanwhile the panel (live engine) keeps sound B");
  S.stop();

  // ---- playback starting inside take 1 (played before), after the sweep: right from the first block
  //      (the playback engine had take 1's end, or another take, and Logic reset it)
  S.startPlay(10.0, 9); S.run(0.6); S.stop();                     // the engine last played take 2 (sound B)
  S.startPlay(4.6, 1);
  S.run(0.02);
  check(S.compare(S.liveAtSong(4.6), "start inside take 1") == 0, "start inside take 1: its sound from the first moment (no chase needed)");
  S.stop();
  S.startPlay(1.2, 2); S.run(0.02);
  check(S.compare(S.liveAtSong(1.2), "start inside take 1 before the sweep") == 0, "start inside take 1 before its sweep: sound A at once");
  // ---- jumping while playing, from take 1 into take 2 and back
  S.jump(11.0); S.run(0.02);
  check(S.compare(S.liveAtSong(10.9), "jump into take 2") == 0, "jump into take 2 while playing: sound B at once");
  S.jump(2.0); S.run(0.02);
  check(S.compare(S.liveAtSong(2.0), "jump back into take 1") == 0, "jump back into take 1: sound A at once");
  S.stop();
  // ---- take 2: sound B, then the mono cable
  S.startPlay(10.0, 9); S.run(0.6);
  check(S.compare(S.liveAtSong(10.5), "take 2") == 0, "take 2 plays with sound B");
  S.run(1.6);
  check(S.compare(S.liveAtSong(12.0), "take 2 after the cable") == 0, "take 2: the patch cable is in the region too");
  S.stop();
  // ---- start inside take 2 after the cable (chase), then take 1 again: the mono flag goes off again
  S.startPlay(12.2, 3); S.run(0.3);
  check(S.compare(S.liveAtSong(12.2), "inside take 2") == 0, "start inside take 2 after the cable");
  S.stop();
  S.startPlay(0.0, 4); S.run(2.5);
  check(S.compare(S.liveAtSong(2.4), "take 1 again") == 0, "back to take 1: sound A again");
  S.stop();

  // ---- a knob turned while take 1 plays back: the panel changes, take 1 does not (the echo is ignored)
  S.startPlay(0.0, 5); S.run(2.2);
  S.knob(0x4A, 3); S.run(0.4);
  check(S.compare(S.liveAtSong(2.4), "knob during playback") == 0, "turning a knob while listening never changes what the take plays");
  S.stop();

  // ---- split take 1 at 4.5 s (inside the held note, after the sweep) and move the second half to 30 s:
  //      after at most a second it plays with take 1's sound at that point, though take 2 (sound B) comes before it
  { std::vector<Ev> moved;
    for (auto &e : S.region) if (e.t >= (int64_t)(4.5 * SR) && e.t < (int64_t)(7.0 * SR)) { Ev m = e; m.t += (int64_t)(25.5 * SR); moved.push_back(m); }
    S.region.insert(S.region.end(), moved.begin(), moved.end());
    S.startPlay(29.0, 6); S.run(2.5);
    check(S.compare(S.liveAtSong(5.9), "split piece") == 0, "a split piece of take 1, moved after take 2, plays with take 1's sound");
    S.stop();
    // (before the piece's first snapshot - in its first second - it cannot be known: as when playing into it)
    S.startPlay(31.2, 7); S.run(0.02);
    check(S.compare(S.liveAtSong(5.7), "inside the moved piece") == 0, "once played, a start inside the moved piece has its sound at once");
    S.stop(); }

  // ---- a take recorded without the plugin's port (track input not "All"; Ableton): it plays with the panel's
  //      sound, like any synth - not with the sound of whatever region came before
  dial(S, 11);
  S.recordPort = false;
  S.startRecord(40.0); S.run(0.3); S.note(67, true); S.run(1.5); S.note(67, false); S.run(0.3); S.stop();
  S.recordPort = true;
  dial(S, 13);
  std::vector<int32_t> soundD(NPARAMS); for (int i = 0; i < NPARAMS; i++) soundD[i] = S.live.getParamRaw(i);
  S.startPlay(40.0, 8); S.run(1.8);
  check(S.compare(soundD, "region without sound") == 0, "a region without recorded sound plays with the panel's sound");
  S.stop();
  // and the regions with sound still play their own (following the panel never kicks in inside them)
  S.startPlay(0.0, 9); S.run(4.5);
  check(S.compare(S.liveAtSong(4.4), "take 1 after the panel-following region") == 0, "take 1 still plays its own sound afterwards");
  S.stop();

  // ---- encoding round trip for every setting
  int rt = 0;
  for (int i = 0; i < NPARAMS; i++) {
    int32_t lo = -(64 << 21), hi = 64 << 21;
    if (S.ps.kind[i] == ParamStream<NPARAMS>::UNI) lo = 0;
    if (S.ps.kind[i] == ParamStream<NPARAMS>::INTV) { lo = 0; hi = 13; }
    for (int k = 0; k <= 8; k++) {
      const int32_t v = lo + (int32_t)((int64_t)(hi - lo) * k / 8);
      const int32_t back = S.ps.raw(i, S.ps.v14(i, v));
      const int64_t d = (int64_t)back - v; if ((S.ps.kind[i] == ParamStream<NPARAMS>::INTV) ? d != 0 : (d > 16500 || d < -16500)) rt++;
    }
  }
  check(rt == 0, "every setting survives the 14-bit stream (knobs within 1/16000, switches exactly)");
  int dup = 0; for (int a = 0; a < ParamStream<NPARAMS>::NSLOT; a++) for (int b = a + 1; b < ParamStream<NPARAMS>::NSLOT; b++) if (ParamStream<NPARAMS>::CcOf(a) == ParamStream<NPARAMS>::CcOf(b)) dup++;
  check(dup == 0 && ParamStream<NPARAMS>::CcOf(ParamStream<NPARAMS>::NSLOT - 1) > 0, "every setting has its own controller number");

  printf("\n%s\n", fails ? "SOME REGION CHECKS FAILED" : "ALL REGION CHECKS PASSED");
  return fails ? 1 : 0;
}
