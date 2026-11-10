// recall_test.cpp - performance recall, tested with a simulated DAW transport.
// Build: clang++ -std=c++17 -O2 -w -o recall_test recall_test.cpp && ./recall_test
//
// Model (engine/recall.h): the host only says "playing". Knobs and cables come straight from the controller
// (directIn); the track carries notes, the switch (pressure) and the mod wheel, and its knob CCs are ignored.
// Where nothing is stored, settings are stored while the transport runs; stored settings play back exactly
// until a knob is moved while playing (touch to overwrite).
#define MS20HOST_TESTING
#include "../engine/recall.h"
#include <stdio.h>
#include <vector>
#include <functional>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

static const int SR = 48000, BLK = 256;
static const double TEMPO = 120.0, PPS = TEMPO / 60.0 / SR;   // beats per sample
typedef std::vector<std::vector<int32_t>> Trace;

struct Daw {
  MS20Host h; Recall r; float L[BLK], R[BLK];
  bool direct = true;                          // the plugin found the controller's port
  bool logic = false;                          // report the beat like Logic does to a live track: same value for 4 blocks, then catch up
  struct M { uint8_t s, d1, d2; }; std::vector<M> dq;   // direct-port queue (CoreMIDI thread -> audio)
  Daw() { h.setHook(&r); h.reset(SR); }
  // from the track (region replay or live): notes, pressure, wheel; knob CCs only matter without the direct port
  void track(uint8_t s, uint8_t d1, uint8_t d2) { if (r.midiIn(0, s, d1, d2, direct)) h.midi(0, s, d1, d2); }
  // a knob on the controller: goes to the direct port and (as Logic records it) to the track as well
  void knob(int c, int v) { if (direct) dq.push_back({0xB0, (uint8_t)c, (uint8_t)v}); track(0xB0, c, v); }
  Trace run(bool running, double ppqStart, int nblk, std::function<void(int)> ev) {
    Trace tr;
    for (int b = 0; b < nblk; b++) {
      if (ev) ev(b);
      const int rb = logic ? b - (b % 4) : b;
      r.beginBlock(&h, running, ppqStart + rb * BLK * PPS, PPS, BLK);
      for (auto &m : dq) { r.directIn(m.s, m.d1, m.d2); h.midi(0, m.s, m.d1, m.d2); }
      dq.clear();
      h.process(L, R, BLK);
      std::vector<int32_t> v(NPARAMS); for (int i = 0; i < NPARAMS; i++) v[i] = h.getParamRaw(i);
      tr.push_back(v);
      if (b % 8 == 0) r.idle();
    }
    r.idle();
    return tr;
  }
};

static int diffBlocks(const Trace &a, int a0, const Trace &b, int b0, int n, int *firstBad = nullptr) {
  int bad = 0; for (int i = 0; i < n; i++) if (a[a0 + i] != b[b0 + i]) { if (!bad && firstBad) *firstBad = i; bad++; }
  return bad;
}

static void suite(bool direct, bool logic) {
  printf("\n--- %s%s ---\n", direct ? "knobs straight from the controller (Mac)" : "knobs through the track (fallback)", logic ? ", beat reported like Logic" : "");
  Daw d; d.direct = direct; d.logic = logic; int fb = -1;
  const int NB = 375;   // 8 beats at 120 bpm in blocks of 256
  // starting sound A, set while stopped
  d.run(false, 0, 20, [&](int b) { if (b == 0) { d.knob(74, 30); d.knob(71, 80); d.knob(28, 10); d.knob(77, 5); d.knob(20, 127); d.knob(21, 64); d.knob(91, 40); d.knob(88, 50); } });

  // the performance. knob() = hands on the controller (Logic records the CCs too)
  auto perform = [&](int b, bool live) {
    auto k = [&](int c, int v) { if (live) d.knob(c, v); else d.track(0xB0, c, v); };   // replay: CCs from the region only
    if (b >= 20 && b < 120 && b % 4 == 0) k(74, 30 + (b - 20) / 2);          // filter sweep
    if (b == 60) d.track(0x90, 48, 100);
    if (b == 90) d.track(0x80, 48, 0);
    if (b == 130) { k(99, 4); k(98, 31); k(6, 25); }                          // a patch cable
    if (b == 170) d.track(0xD0, 100, 0);                                      // memory recall: switch + key
    if (b == 172) d.track(0x90, 50, 100);
    if (b == 180) { d.track(0x80, 50, 0); d.track(0xD0, 0, 0); }
    if (b >= 220 && b < 300 && b % 5 == 0) k(89, 20 + (b - 220));            // feedback (catch-up after the recall)
    if (b == 320) k(11, 100);                                                // play mode to latch
  };
  auto rec = d.run(true, 0.0, NB, [&](int b) { perform(b, true); });
  d.run(false, 0, 10, nullptr);
  check(d.r.takes() == 1, "the take is stored (one part, not one per block)");
  check(d.r.laneForTest().ev.size() < 2000, "a sensible number of stored changes");
  bool moved = false; for (int i = 0; i < NPARAMS; i++) if (rec.front()[i] != rec.back()[i]) moved = true;
  check(moved, "settings changed during the take");

  // afterwards, stopped: the player turns knobs to something else
  d.run(false, 0, 20, [&](int b) { if (b == 0) { d.knob(74, 120); d.knob(71, 0); d.knob(28, 90); d.knob(91, 0); d.knob(88, 10); d.knob(89, 5); d.knob(11, 0); } });
  const size_t ev0 = d.r.laneForTest().ev.size();

  // 1. play back from the start: the region replays notes, switch and its recorded CCs
  auto replay = [&](int b) { perform(b, false); };
  auto pb = d.run(true, 0.0, NB, replay);
  int bad = diffBlocks(rec, 0, pb, 0, NB, &fb);
  printf("      playback from the start: %d of %d blocks differ (first %d)\n", bad, NB, fb);
  check(bad == 0, "playback = the take, every block, whatever the knobs are now");
  check(d.r.takes() == 1 && d.r.laneForTest().ev.size() == ev0, "listening changes nothing that is stored");

  // 2. from the middle
  d.run(false, 0, 10, [&](int b) { if (b == 0) d.knob(74, 5); });
  const int S = 188;
  auto pm = d.run(true, S * BLK * PPS, NB - S, [&](int b) { replay(b + S); });
  bad = diffBlocks(rec, S, pm, 0, NB - S, &fb);
  printf("      playback from beat 4: %d of %d blocks differ (first %d)\n", bad, NB - S, fb);
  check(bad == 0, "playback from the middle = the take");

  // 3. save and load into a new instance
  auto blob = d.r.serialize();
  Daw e; e.direct = direct; e.logic = logic; e.r.unserialize(blob.data(), blob.size());
  e.run(false, 0, 10, [&](int b) { if (b == 0) { e.knob(74, 3); e.knob(91, 120); } });
  auto pe = e.run(true, 0.0, NB, [&](int b) { /* the same region, replayed into the new instance */
    auto k = [&](int c, int v) { e.track(0xB0, c, v); };
    if (b >= 20 && b < 120 && b % 4 == 0) k(74, 30 + (b - 20) / 2);
    if (b == 60) e.track(0x90, 48, 100); if (b == 90) e.track(0x80, 48, 0);
    if (b == 130) { k(99, 4); k(98, 31); k(6, 25); }
    if (b == 170) e.track(0xD0, 100, 0); if (b == 172) e.track(0x90, 50, 100);
    if (b == 180) { e.track(0x80, 50, 0); e.track(0xD0, 0, 0); }
    if (b >= 220 && b < 300 && b % 5 == 0) k(89, 20 + (b - 220));
    if (b == 320) k(11, 100); });
  bad = diffBlocks(rec, 0, pe, 0, NB, &fb);
  printf("      after save/load, new instance: %d of %d blocks differ (first %d)\n", bad, NB, fb);
  check(bad == 0, "saved take plays back the same in a new instance");

  // 4. touch to overwrite: playing back, a knob moves at block T
  const int T = 250;
  auto touch = [&](int b) { if (b < T) replay(b); if (b == T) d.knob(71, 7); if (b > T && b % 10 == 0) d.knob(74, (b * 3) % 128); };
  auto tw = d.run(true, 0.0, NB, touch);
  d.run(false, 0, 10, [&](int b) { if (b == 0) d.knob(74, 1); });
  auto region2 = [&](int b) { if (b < T) replay(b); if (b == T) d.track(0xB0, 71, 7); if (b > T && b % 10 == 0) d.track(0xB0, 74, (b * 3) % 128); };
  auto pt = d.run(true, 0.0, NB, region2);
  int before = diffBlocks(rec, 0, pt, 0, T), after = diffBlocks(tw, T + 1, pt, T + 1, NB - T - 1, &fb);
  printf("      touch at block %d: before %d differ (old), after %d differ (new, first %d)\n", T, before, after, fb);
  check(before == 0 && after == 0, "a knob moved during playback replaces the settings from that point");
  check(tw[T + 5] != rec[T + 5], "the new settings really are different");

  // 5. after the stored part: new ground is stored too (and plays back)
  const double P2 = NB * BLK * PPS + 2.0;
  d.run(false, 0, 10, [&](int b) { if (b == 0) d.knob(28, 33); });
  auto fresh = d.run(true, P2, 100, [&](int b) { if (b == 40) d.knob(74, 90); });
  d.run(false, 0, 10, [&](int b) { if (b == 0) { d.knob(28, 99); d.knob(74, 2); } });
  auto pf = d.run(true, P2, 100, nullptr);
  check(diffBlocks(fresh, 0, pf, 0, 100) == 0, "a new part is stored from where it starts and plays back");

  // 6. clear
  d.r.clear(); d.r.idle();
  check(d.r.takes() == 0, "clear removes all stored settings");
}

int main() {
  suite(true, false);
  suite(false, false);
  suite(true, true);
  suite(false, true);
  printf(fails ? "\n%d RECALL CHECKS FAILED\n" : "\nALL RECALL CHECKS PASSED\n", fails);
  return fails ? 1 : 0;
}
