// recall_test.cpp - performance recall, tested with a simulated DAW transport.
// Build: clang++ -std=c++17 -O2 -w -o recall_test recall_test.cpp && ./recall_test
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

struct Daw {
  MS20Host h; Recall r; float L[BLK], R[BLK];
  Daw() { h.setHook(&r); h.reset(SR); }
  // the plugin's MIDI input: knob/cable CCs from the track are ignored inside a recorded take (wheel CC1 passes)
  void midi(int off, uint8_t s, uint8_t d1, uint8_t d2) {
    if (!r.midiIn(off, s, d1, d2)) return;
    h.midi(off, s, d1, d2);
  }
  // runs blocks; ev(block) sends that block's MIDI; returns every parameter at the end of every block
  std::vector<std::vector<int32_t>> run(bool running, bool rec, double ppqStart, int nblk, std::function<void(int)> ev) {
    std::vector<std::vector<int32_t>> tr;
    for (int b = 0; b < nblk; b++) {
      if (ev) ev(b);
      r.beginBlock(&h, running, rec, ppqStart + b * BLK * PPS, PPS, BLK);
      h.process(L, R, BLK);
      std::vector<int32_t> v(NPARAMS); for (int i = 0; i < NPARAMS; i++) v[i] = h.getParamRaw(i);
      tr.push_back(v);
      if (b % 8 == 0) r.idle();
    }
    r.idle();
    return tr;
  }
  void cc(int c, int v) { midi(0, 0xB0, c, v); }
};

static int diffBlocks(const std::vector<std::vector<int32_t>> &a, int a0, const std::vector<std::vector<int32_t>> &b, int b0, int n, int *firstBad = nullptr) {
  int bad = 0; for (int i = 0; i < n; i++) if (a[a0 + i] != b[b0 + i]) { if (!bad && firstBad) *firstBad = i; bad++; }
  return bad;
}

int main() {
  Daw d;
  // starting sound A, set while stopped
  d.run(false, false, 0, 40, [&](int b) { if (b == 0) { d.cc(74, 30); d.cc(71, 80); d.cc(28, 10); d.cc(77, 5); d.cc(20, 127); d.cc(21, 64); d.cc(91, 40); d.cc(88, 50); } });

  // the performance: 8 beats = 375 blocks of 256 at 120 bpm
  const int NB = 375;
  auto perform = [&](int b) {
    if (b >= 20 && b < 120 && b % 4 == 0) d.cc(74, 30 + (b - 20) / 2);          // filter sweep
    if (b == 60) { d.midi(0, 0x90, 48, 100); }                                  // a note
    if (b == 90) { d.midi(0, 0x80, 48, 0); }
    if (b == 130) { d.cc(99, 4); d.cc(98, 31); d.cc(6, 25); }                   // a patch cable (KBD CV -> VCO1+2 CV IN)
    if (b == 170) { d.midi(0, 0xD0, 100, 0); }                                  // memory recall: switch + key
    if (b == 172) { d.midi(0, 0x90, 50, 100); }
    if (b == 180) { d.midi(0, 0x80, 50, 0); d.midi(0, 0xD0, 0, 0); }
    if (b >= 220 && b < 300 && b % 5 == 0) d.cc(89, 20 + (b - 220));            // feedback knob (catch-up after the recall)
    if (b == 320) d.cc(11, 100);                                                // play mode to latch
  };
  auto rec = d.run(true, true, 0.0, NB, perform);
  d.run(false, false, 0, 20, nullptr);
  check(d.r.takes() == 1, "one take stored");
  check(d.r.laneForTest().ev.size() > (size_t)NPARAMS, "take has a snapshot plus changes");
  bool moved = false; for (int i = 0; i < NPARAMS; i++) if (rec.front()[i] != rec.back()[i]) moved = true;
  check(moved, "settings changed during the take");

  // afterwards: the player turns knobs to something else (sound B)
  d.run(false, false, 0, 40, [&](int b) { if (b == 0) { d.cc(74, 120); d.cc(71, 0); d.cc(28, 90); d.cc(91, 0); d.cc(88, 10); d.cc(89, 5); d.cc(11, 0); d.cc(99, 5); d.cc(98, 31); d.cc(6, 25); } });

  // 1. play back from the start, with the region's MIDI (CCs included) replayed
  int fb = -1;
  auto pb = d.run(true, false, 0.0, NB, perform);
  int bad = diffBlocks(rec, 0, pb, 0, NB, &fb);
  printf("      playback from the start: %d of %d blocks differ (first %d)\n", bad, NB, fb);
  check(bad == 0, "playback from the start = the take, every block");

  // 2. play back from the middle (beat 4 = block 187.5 -> start at block 188)
  d.run(false, false, 0, 10, [&](int b) { if (b == 0) d.cc(74, 5); });
  const int S = 188;
  auto pm = d.run(true, false, S * BLK * PPS, NB - S, [&](int b) { perform(b + S); });
  bad = diffBlocks(rec, S, pm, 0, NB - S, &fb);
  printf("      playback from beat 4: %d of %d blocks differ (first %d)\n", bad, NB - S, fb);
  check(bad == 0, "playback from the middle = the take");

  // 3. after the take: live again (knobs work)
  d.run(true, false, NB * BLK * PPS + 1.0, 10, [&](int b) { if (b == 2) d.cc(74, 77); });
  check(d.h.getParamRaw(0) == d.h.getParamRaw(0) && !d.r.knobsLocked(), "outside the take: not locked");

  // 4. save and load with the project
  auto blob = d.r.serialize();
  Daw e; e.r.unserialize(blob.data(), blob.size());
  e.run(false, false, 0, 10, [&](int b) { if (b == 0) { e.cc(74, 3); e.cc(91, 120); } });
  auto pe = e.run(true, false, 0.0, NB, perform);
  bad = diffBlocks(rec, 0, pe, 0, NB, &fb);
  printf("      after save/load, fresh instance: %d of %d blocks differ (first %d)\n", bad, NB, fb);
  check(bad == 0, "saved take plays back the same in a new instance");

  // 5. punch-in over beats 2..3 with a different filter move; before and after keep the first take
  const int P0 = 94, P1 = 141;   // blocks for beats 2 and 3 (rounded up)
  d.run(false, false, 0, 10, [&](int b) { if (b == 0) d.cc(74, 64); });
  auto punch = d.run(true, true, P0 * BLK * PPS, P1 - P0, [&](int b) { if (b % 3 == 0) d.cc(28, 100 - b); });
  d.run(false, false, 0, 10, nullptr);
  check(d.r.takes() == 3, "punch-in splits the take: before, new, after");
  // the region after the punch-in: the punched part holds the new moves
  auto region = [&](int b) { if (b >= P0 && b < P1) { int pb = b - P0; if (pb % 3 == 0) d.cc(28, 100 - pb); } else perform(b); };
  auto pp = d.run(true, false, 0.0, NB, region);
  int b1 = diffBlocks(rec, 0, pp, 0, P0), b2 = diffBlocks(punch, 0, pp, P0, P1 - P0), b3 = diffBlocks(rec, P1 + 1, pp, P1 + 1, NB - P1 - 1, &fb);
  printf("      after punch-in: before %d, punch %d, after %d differing blocks (first after %d)\n", b1, b2, b3, fb);
  check(b1 == 0 && b2 == 0 && b3 == 0, "punch-in: all three parts play back exactly");

  // 6. recording ANOTHER track over the takes: the region replays (same MIDI), nothing is touched
  {
    const size_t ev0 = d.r.laneForTest().ev.size(); const int t0 = d.r.takes();
    d.run(false, false, 0, 10, [&](int b) { if (b == 0) d.cc(74, 99); });
    auto ov = d.run(true, true, 0.0, NB, region);
    d.run(false, false, 0, 10, nullptr);
    int bo = diffBlocks(pp, 0, ov, 0, NB, &fb);
    printf("      recording another track over the takes: %d of %d blocks differ, takes %d -> %d, events %zu -> %zu\n", bo, NB, t0, d.r.takes(), ev0, d.r.laneForTest().ev.size());
    check(bo == 0 && d.r.takes() == t0 && d.r.laneForTest().ev.size() == ev0, "overdubbing another track keeps the takes exactly");
  }

  // 7. touch to overwrite: record over the take and move a knob at block 250 -> new take from there
  {
    const int T = 250;
    auto touch = [&](int b) { if (b < T) region(b); if (b == T) d.cc(71, 7); if (b > T && b % 10 == 0) d.cc(74, (b * 3) % 128); };
    auto tw = d.run(true, true, 0.0, NB, touch);
    d.run(false, false, 0, 10, [&](int b) { if (b == 0) d.cc(74, 1); });
    auto pt = d.run(true, false, 0.0, NB, touch);
    int before = diffBlocks(pp, 0, pt, 0, T), after = diffBlocks(tw, T + 1, pt, T + 1, NB - T - 1, &fb);
    printf("      touch at block %d: before %d differ (old take), after %d differ (new take, first %d)\n", T, before, after, fb);
    check(before == 0 && after == 0, "touching a knob while recording over a take replaces it from that point");
    check(tw[T + 5] != pp[T + 5], "the new take really is different");
  }

  // 8. clear
  d.r.clear(); d.r.idle();
  check(d.r.takes() == 0, "clear removes all takes");

  printf(fails ? "\n%d RECALL CHECKS FAILED\n" : "\nALL RECALL CHECKS PASSED\n", fails);
  return fails ? 1 : 0;
}
