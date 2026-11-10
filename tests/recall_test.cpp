// recall_test.cpp - two-engine performance recall, tested against a simulated Logic.
// Build: clang++ -std=c++17 -O2 -w -o recall_test recall_test.cpp && ./recall_test
//
// The live engine is played straight from the controller; Logic also sends the live playing through the
// track (a moment later) and records it into a region. The playback engine plays the track: it follows the
// take a region's notes belong to, or mirrors the panel. Logic reports the beat of a live track in steps.
#define MS20HOST_TESTING
#include "../engine/recall.h"
#include <stdio.h>
#include <vector>
#include <functional>
#include <deque>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

static const int SR = 48000, BLK = 256;
static const double TEMPO = 120.0, PPS = TEMPO / 60.0 / SR;
typedef std::vector<std::vector<int32_t>> Trace;
struct M { uint8_t s, d1, d2; };

struct Plug {   // the plugin, as wired in TwentyEngine.cpp
  MS20Host liveE, playE; Recall r; float L[BLK], R[BLK];
  Plug() { liveE.reset(SR); playE.reset(SR); r.setEngines(&liveE, &playE); }
  std::vector<M> direct, track;   // this block's MIDI: straight from the controller, and from Logic's track
  void block(bool running, double hostPpq) {
    for (auto &m : direct) { if (r.directMsg(m.s, m.d1, m.d2)) playE.midi(0, 0x80, m.d1, 0); liveE.midi(0, m.s, m.d1, m.d2); }
    for (auto &m : track) {
      if ((m.s & 0xF0) == 0xB0 && m.d1 != 1) continue;            // knob data on the track: ignored
      if (r.trackIsLive(m.s, m.d1, m.d2)) continue;                // Logic's copy of live playing
      if ((m.s & 0xF0) == 0x90 && m.d2 > 0) r.trackNoteOn(0, m.d1);
      playE.midi(0, m.s, m.d1, m.d2);
    }
    direct.clear(); track.clear();
    r.beginBlock(running, hostPpq, PPS, BLK);
    r.setChunkOffset(0);
    liveE.process(L, R, BLK); playE.process(L, R, BLK);
  }
};

struct Host { bool stepped; int late; bool echo; bool recTrue; const char *name; };   // recTrue: the region gets the played time
static Host H = {true, 1, true, false, "Logic"};
struct Logic {   // the host: transport, a region, live input routed to the track H.late blocks later (if H.echo)
  Plug p; bool steppedBeat = H.stepped; std::deque<std::vector<M>> pipe, dpipe;
  std::vector<std::vector<M>> region;   // what the region holds, per block (recorded from the live input)
  // run nblk blocks from block b0; live(b) plays the controller; recordRegion: Logic records the live input
  struct Out { Trace live, play; };
  Out run(bool running, int b0, int nblk, std::function<void(int, std::vector<M> &)> live, bool playRegion, bool recordRegion) {
    Out o;
    for (int i = 0; i < nblk; i++) {
      const int b = b0 + i;
      std::vector<M> now; if (live) live(b, now);
      std::vector<M> arrived;
      if (H.late >= 0) {
        for (auto &m : now) p.direct.push_back(m);
        pipe.push_back(now);
        if ((int)pipe.size() > H.late) { arrived = pipe.front(); pipe.pop_front(); }
      } else {   // the host's copy is first; the plugin's own read of the controller comes -late blocks later
        arrived = now;
        dpipe.push_back(now);
        if ((int)dpipe.size() > -H.late) { for (auto &m : dpipe.front()) p.direct.push_back(m); dpipe.pop_front(); }
      }
      if (H.echo) for (auto &m : arrived) p.track.push_back(m);          // the host passes the live input on to the plugin
      if (recordRegion && running) { if ((int)region.size() <= b) region.resize(b + 1); for (auto &m : (H.recTrue ? now : arrived)) region[b].push_back(m); }
      if (playRegion && running && b < (int)region.size()) for (auto &m : region[b]) p.track.push_back(m);
      const int rb = steppedBeat ? b - (b % 4) : b;
      p.block(running, rb * BLK * PPS);
      std::vector<int32_t> lv(NPARAMS), pv(NPARAMS);
      for (int k = 0; k < NPARAMS; k++) { lv[k] = p.liveE.getParamRaw(k); pv[k] = p.playE.getParamRaw(k); }
      o.live.push_back(lv); o.play.push_back(pv);
      if (i % 8 == 0) p.r.idle();
    }
    p.r.idle();
    return o;
  }
  void stopped(std::function<void(std::vector<M> &)> f, int n = 10) { run(false, 0, n, [&](int b, std::vector<M> &v) { if (b == 0 && f) f(v); }, false, false); }
};

static int diff(const Trace &a, int a0, const Trace &b, int b0, int n, int *first = nullptr) {
  int bad = 0; for (int i = 0; i < n; i++) if (a[a0 + i] != b[b0 + i]) { if (!bad && first) *first = i; bad++; }
  return bad;
}
static void cc(std::vector<M> &v, int c, int x) { v.push_back({0xB0, (uint8_t)c, (uint8_t)x}); }

static int suite() {
  fails = 0;
  Logic L; int fb = -1;
  // a region note arrives LAG blocks after it was played; before it, nothing sounds on the playback engine
  const int LAG = H.recTrue ? 1 : (H.late > 1 ? H.late : 1);
  const int NB = 375, FIRST = 12;   // 8 beats; the first note at block 12
  L.stopped([](std::vector<M> &v) { cc(v, 74, 30); cc(v, 71, 80); cc(v, 28, 10); cc(v, 77, 5); cc(v, 20, 127); cc(v, 21, 64); cc(v, 91, 40); cc(v, 88, 50); });

  // the performance on the controller
  auto perform = [&](int b, std::vector<M> &v) {
    if (b == FIRST) v.push_back({0x90, 48, 100});
    if (b >= 20 && b < 120 && b % 4 == 0) cc(v, 74, 30 + (b - 20) / 2);
    if (b == 60) v.push_back({0x80, 48, 0});
    if (b == 64) v.push_back({0x90, 52, 90});
    if (b == 100) v.push_back({0x80, 52, 0});
    if (b == 130) { cc(v, 99, 4); cc(v, 98, 31); cc(v, 6, 25); }
    if (b == 170) v.push_back({0xD0, 100, 0});                       // memory recall: switch + key
    if (b == 172) v.push_back({0x90, 50, 100});
    if (b == 180) { v.push_back({0x80, 50, 0}); v.push_back({0xD0, 0, 0}); }
    if (b == 200) v.push_back({0x90, 55, 100});
    if (b >= 220 && b < 300 && b % 5 == 0) cc(v, 89, 20 + (b - 220));
    if (b == 300) v.push_back({0x80, 55, 0});
    if (b == 310) v.push_back({0x90, 60, 70});
    if (b == 320) cc(v, 11, 100);
    if (b == 360) v.push_back({0x80, 60, 0});
  };
  auto rec = L.run(true, 0, NB, perform, false, true);
  L.stopped(nullptr);
  check(L.p.r.takes() == 1, "the performance is stored as one take");
  bool moved = false; for (int k = 0; k < NPARAMS; k++) if (rec.live[FIRST + 1 + LAG][k] != rec.live[NB - 1][k]) moved = true;
  check(moved, "the settings moved during the take");
  check(diff(rec.live, 0, rec.play, 0, NB) == 0, "while recording, the playback engine mirrors the panel");

  // afterwards, stopped: the panel goes somewhere else
  std::vector<M> panelB; cc(panelB, 74, 120); cc(panelB, 71, 0); cc(panelB, 28, 90); cc(panelB, 91, 0); cc(panelB, 88, 10); cc(panelB, 89, 5); cc(panelB, 11, 0);
  L.stopped([&](std::vector<M> &v) { v = panelB; });
  const std::vector<int32_t> panel = [&] { std::vector<int32_t> v(NPARAMS); for (int k = 0; k < NPARAMS; k++) v[k] = L.p.liveE.getParamRaw(k); return v; }();

  // 1. play the region from the start
  auto pb = L.run(true, 0, NB, nullptr, true, false);
  int bad = diff(rec.live, FIRST + 1 + LAG, pb.play, FIRST + 1 + LAG, NB - FIRST - 1 - LAG, &fb);
  printf("      playback engine vs the performance (from the first note): %d of %d blocks differ (first %d)\n", bad, NB - FIRST - 1 - LAG, fb);
  check(bad == 0, "the take plays back exactly as performed");
  bool liveKept = true; for (auto &v : pb.live) if (v != panel) liveKept = false;
  check(liveKept, "the live engine (the panel) is untouched during playback");
  check(L.p.r.takes() == 1, "listening stores nothing");

  // 2. from the middle (beat 4)
  const int S = 188;
  auto pm = L.run(true, S, NB - S, nullptr, true, false);
  const int NEXT = 200 - S + 1 + LAG;   // the take is picked up at the next note of the region (block 200)
  bad = diff(rec.live, S + NEXT, pm.play, NEXT, NB - S - NEXT, &fb);
  printf("      from beat 4, after the next note: %d of %d blocks differ (first %d)\n", bad, NB - S - NEXT, fb);
  check(bad == 0, "from the middle, exact from the next note on");

  // 3. a deleted region: the playback engine just mirrors the panel
  auto saved = L.region; L.region.clear();
  auto pd = L.run(true, 0, NB, nullptr, true, false);
  bool mirror = true; for (int i = 0; i < NB; i++) if (pd.play[i] != pd.live[i]) mirror = false;
  check(mirror && !L.p.r.followingForTest(), "deleted region: nothing is recalled, the panel plays");
  // 4. a different region (other notes): also the panel
  L.region.assign(NB, {}); L.region[30].push_back({0x90, 70, 100}); L.region[90].push_back({0x80, 70, 0});
  auto po = L.run(true, 0, NB, nullptr, true, false);
  mirror = true; for (int i = 0; i < NB; i++) if (po.play[i] != po.live[i]) mirror = false;
  check(mirror, "a different region plays with the panel settings");
  L.region = saved;

  // 5. jamming live over the playing take: the take is untouched, the jam becomes its own take
  auto jam = [&](int b, std::vector<M> &v) { if (b == 40) v.push_back({0x90, 67, 100}); if (b >= 41 && b < 90 && b % 3 == 0) cc(v, 28, b); if (b == 90) v.push_back({0x80, 67, 0}); };
  auto pj = L.run(true, 0, NB, jam, true, false);
  bad = diff(rec.live, FIRST + 1 + LAG, pj.play, FIRST + 1 + LAG, NB - FIRST - 1 - LAG, &fb);
  L.stopped(nullptr);
  printf("      take while jamming over it: %d blocks differ (first %d); takes now %d\n", bad, fb, L.p.r.takes());
  check(bad == 0 && L.p.r.takes() == 2, "a jam over a playing take does not change it (it is stored as its own take)");
  auto pa = L.run(true, 0, NB, nullptr, true, false);
  check(diff(rec.live, FIRST + 1 + LAG, pa.play, FIRST + 1 + LAG, NB - FIRST - 1 - LAG) == 0, "afterwards the region still plays its own take");

  // 6. save and load into a new plugin
  auto blob = L.p.r.serialize();
  Logic L2; L2.region = L.region; L2.p.r.unserialize(blob.data(), blob.size());
  L2.stopped([&](std::vector<M> &v) { cc(v, 74, 3); cc(v, 91, 120); });
  auto p2 = L2.run(true, 0, NB, nullptr, true, false);
  bad = diff(rec.live, FIRST + 1 + LAG, p2.play, FIRST + 1 + LAG, NB - FIRST - 1 - LAG, &fb);
  printf("      after save/load: %d blocks differ (first %d)\n", bad, fb);
  check(bad == 0, "saved with the project, it plays back the same");

  // 7. live notes are not played twice (Logic's copy of live playing is recognised)
  Logic L3;
  L3.run(true, 0, 40, [](int b, std::vector<M> &v) { if (b == 5) v.push_back({0x90, 60, 100}); if (b == 30) v.push_back({0x80, 60, 0}); }, false, true);
  check(!L3.p.r.followingForTest(), "live playing goes only to the live engine");

  // 8. clear
  L.p.r.clear(); L.p.r.idle();
  check(L.p.r.takes() == 0, "clear removes all takes");

  return fails;
}

int main() {
  // Logic reports the beat in steps and passes live input on a block later; REAPER, Ableton, Bitwig and Cubase
  // report it smoothly, with the live input in the same block, a few blocks later, or not at all (not monitoring).
  const Host hosts[] = {{true, 1, true, false, "Logic"}, {false, 0, true, true, "smooth beat, same block"},
                        {false, 3, true, true, "smooth beat, monitoring 3 blocks late"}, {false, 1, false, true, "track not monitoring"},
                        {true, 4, true, true, "stepped beat, monitoring 4 blocks late"},
                        {false, 3, true, false, "smooth beat, notes recorded 3 blocks late"},
                        {false, -1, true, true, "host copy a block before the plugin's own read"}};
  int total = 0;
  for (const Host &h : hosts) {
    H = h; printf("\n==== host: %s\n", h.name);
    const int f = suite(); total += f;
    printf("==== %s: %s\n", h.name, f ? "FAILED" : "passed");
  }
  printf(total ? "\n%d RECALL CHECKS FAILED\n" : "\nALL RECALL CHECKS PASSED\n", total);
  return total ? 1 : 0;
}
