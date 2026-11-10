// automation_test.cpp - the Automation plugin's writing rules (engine/autowrite.h), against a simulated Logic in Latch.
// Build: clang++ -std=c++17 -O2 -w -o automation_test automation_test.cpp && ./automation_test
//
// Logic model (track automation, Latch, never switched to Read):
//  - while it runs, every lane sends its value at the playhead to the plugin, except parameters being written;
//  - a gesture from the plugin while it runs starts writing that parameter (latched until stop);
//    at stop the written part replaces the lane from the first gesture to the stop point, and the lane's
//    old value continues after it;
//  - a gesture while stopped writes a point at the playhead (Latch): the new plugin must never do that.
// The plugin is wired as in TwentyEngine.cpp (TWENTY_STUDIO): two engines, or one engine without the direct
// connection (Windows), and the host's values on the playback engine.
#include "../engine/autowrite.h"
#include <stdio.h>
#include <map>
#include <vector>
#include <string.h>
using namespace ms20;

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

static const int N = 8;          // 0..5 knobs (frac), 6..7 switches (int)
static const int32_t U = 2097152; // one knob unit
static const double STEP = 0.01; // 10 ms per step (one idle round)

struct Logic {
  bool knowsRecording = true;
  std::map<double, int32_t> lane[N];
  bool running = false, recording = false; double pos = 0;
  bool latched[N] = {}; double latchFrom[N] = {}; std::vector<std::pair<double, int32_t>> written[N];
  int stoppedWrites = 0;
  bool laneAt(int i, double t, int32_t &v) const {
    auto it = lane[i].upper_bound(t); if (it == lane[i].begin()) return false; --it; v = it->second; return true;
  }
  void gesture(int i, int32_t v) {
    if (!running) { lane[i][pos] = v; stoppedWrites++; return; }
    if (!latched[i]) { latched[i] = true; latchFrom[i] = pos; }
    written[i].push_back({pos, v});
  }
  void stop() {
    for (int i = 0; i < N; i++) if (latched[i]) {
      int32_t after; const bool had = laneAt(i, pos + 1e-9, after);
      for (auto it = lane[i].lower_bound(latchFrom[i]); it != lane[i].end() && it->first <= pos;) it = lane[i].erase(it);
      for (auto &w : written[i]) lane[i][w.first] = w.second;
      if (had) lane[i][pos + 1e-6] = after;
      latched[i] = false; written[i].clear();
    }
    running = recording = false;
  }
};

struct Plugin {
  AutoWrite<N> aw; bool direct = true;          // direct: the controller reaches the plugin (two engines)
  int32_t live[N] = {}, play[N] = {}, param[N] = {};
  bool wasRec = false, recStartReq = false, writes = false; int startWrote = 0, movesWrote = 0;
  Logic *host;
  Plugin(Logic *h) : host(h) { for (int i = 0; i < N; i++) aw.frac[i] = i < 6; }
  void fromHost(int i, int32_t v) { param[i] = v; play[i] = v; if (!direct) live[i] = v; }   // OnParamChange(kHost)
  void loadProject(const int32_t *v) { for (int i = 0; i < N; i++) { param[i] = v[i]; play[i] = v[i]; if (aw.fromProject(i, v[i]) || !direct) live[i] = v[i]; } }
  void write(int i, int32_t v) { host->gesture(i, v); param[i] = v; play[i] = v; if (!direct) live[i] = v; }  // WriteToHost
  void block() {   // ProcessBlock: the recording flag
    const bool rec = hostRecording(host->running, host->knowsRecording, host->recording);
    if (rec && !wasRec) recStartReq = true; wasRec = rec;
    writes = hostWrites(host->running, host->knowsRecording, host->recording);
  }
  std::vector<std::pair<int, int32_t>> knobs;   // live engine changes waiting for OnIdle
  void idle() {
    const bool rec = writes;
    if (recStartReq) { recStartReq = false; movesWrote = 0; startWrote = aw.recordStart(param, [this](int i, int32_t v) { write(i, v); }); }
    for (auto &k : knobs) { live[k.first] = k.second; if (aw.liveChange(k.first, k.second, rec)) { write(k.first, k.second); movesWrote++; } }
    knobs.clear();
  }
  void knob(int i, int32_t v) { knobs.push_back({i, v}); }
};

struct Sim {
  Logic L; Plugin P{&L};
  void step() {
    if (L.running) for (int i = 0; i < N; i++) { int32_t v; if (!L.latched[i] && L.laneAt(i, L.pos, v)) P.fromHost(i, v); }
    P.block(); P.idle();
    if (L.running) L.pos += STEP;
  }
  void run(double secs) { for (int k = 0; k < (int)(secs / STEP + 0.5); k++) step(); }
  void locate(double t) { L.pos = t; if (L.running) return; for (int i = 0; i < N; i++) { int32_t v; if (L.laneAt(i, t, v)) P.fromHost(i, v); } }   // Logic chases on locate
  void play(bool rec) { L.running = true; L.recording = rec; }
  void stop() { L.stop(); step(); }
  void dial(const int32_t *s) { for (int i = 0; i < N; i++) P.knob(i, s[i]); step(); }
  bool laneIs(double t, const int32_t *s) { for (int i = 0; i < N; i++) { int32_t v; if (!L.laneAt(i, t, v) || v != s[i]) return false; } return true; }
  bool same(const int32_t *a, const int32_t *b) { return memcmp(a, b, sizeof(int32_t) * N) == 0; }
};

static void scenario(bool direct, bool knows) {
  printf("\n--- %s, host %s recording ---\n", direct ? "two engines (Mac)" : "one engine (Windows)", knows ? "reports" : "does not report");
  Sim S; S.P.direct = direct; S.L.knowsRecording = knows;
  const int32_t proj[N] = {10 * U, 20 * U, 30 * U, 40 * U, 50 * U, 60 * U, 1, 0};
  const int32_t A[N]    = {12 * U, 22 * U, 32 * U, 42 * U, 52 * U, 62 * U, 2, 1};
  const int32_t B[N]    = { 5 * U,  6 * U,  7 * U,  8 * U,  9 * U, 10 * U, 0, 0};
  S.P.loadProject(proj); S.step();
  check(S.same(S.P.live, proj), "a loaded project: the panel starts with the project's sound");

  if (!knows) {   // fallback: knob moves written while the host runs (as before), never a start
    for (int i = 0; i < N; i++) S.L.lane[i][0.0] = A[i];
    S.dial(B); S.locate(1.0); S.play(false); S.run(0.5); S.P.knob(2, 33 * U); S.run(0.5); S.stop();
    int32_t v; S.L.laneAt(2, 1.8, v);
    check(v == 33 * U && S.L.stoppedWrites == 0, "host without a recording flag: a knob moved while it runs is written (as before; documented)");
    check(S.P.startWrote == 0 && S.L.laneAt(1, 1.5, v) && v == A[1], "... but nothing else: the panel is never written at play");
    return;
  }
  // take 1: sound A dialled while stopped, recorded from 1.0 to 5.0, with a cutoff sweep in the middle
  S.dial(A);
  check(S.L.stoppedWrites == 0, "dialling a sound while stopped writes nothing");
  S.locate(1.0); S.play(true); S.run(1.0);
  for (int k = 0; k <= 10; k++) { S.P.knob(0, (12 + k) * U); S.run(0.1); }
  S.P.knob(0, A[0]); S.run(1.95); S.stop();
  check(S.laneIs(1.5, A), "take 1: the lanes hold sound A at its start");
  { int32_t v; S.L.laneAt(0, 2.5, v); check(v > A[0], "take 1: the cutoff sweep is in the lane"); }

  // take 2: a new sound B dialled while stopped, then the playhead moved, recorded from 9.0 to 12.0
  S.dial(B); S.locate(9.0);
  check(S.L.stoppedWrites == 0, "new sound + moving the playhead writes nothing");
  check(S.same(S.P.aw.panel, B), "the panel keeps the new sound after locating (Logic chases the old lanes)");
  if (direct) check(S.same(S.P.live, B), "... and the keys play it");
  S.play(true); S.run(3.0); S.stop();
  check(S.laneIs(9.5, B), "take 2: the lanes hold sound B");
  check(S.laneIs(1.5, A), "take 1 is untouched by take 2");

  // Pete's case: back to take 1 and play it. Keys played along, knobs touched (and jitter) while it plays.
  S.locate(1.0); S.play(false); S.run(0.5);
  S.P.knob(2, 33 * U); S.run(0.2); S.P.knob(2, 33 * U + 3); S.run(0.2);
  if (direct) check(S.P.play[1] == A[1], "while take 1 plays, the playback engine has sound A");
  if (direct) check(S.P.live[1] == B[1], "... and the keys still play the panel (sound B)");
  S.run(1.0); S.stop();
  check(S.laneIs(1.5, A) && S.L.stoppedWrites == 0, "playing take 1 back (knobs touched) leaves take 1 as it was");

  // a sweep added to take 2 right after recording it: only the moved knob is written
  S.dial(B);   // the panel back on B exactly (only cutoff moves in this pass)
  S.locate(9.0); S.play(true); S.run(0.5);
  for (int k = 0; k <= 5; k++) { S.P.knob(0, (5 + k) * U); S.run(0.1); }
  S.run(1.0); S.stop();
  check(S.P.startWrote == 0, "recording over a take with its own sound: nothing written at the start");
  { bool others = true; for (int i = 1; i < N; i++) { int32_t v; S.L.laneAt(i, 10.0, v); if (v != B[i]) others = false; }
    int32_t c; S.L.laneAt(0, 9.9, c);
    check(others && c > B[0], "... only the cutoff sweep is written, the rest of take 2 stays"); }

  // a new take between take 1 and take 2 (the lanes there still hold take 1's end) with the panel on B: B is written
  S.locate(6.0); S.play(true); S.run(2.0); S.stop();
  { int32_t c; S.L.laneAt(0, 6.5, c); check(S.P.startWrote > 0 && S.laneIs(6.5, S.P.live), "a take where the lanes hold another sound starts with the panel's sound"); }
  check(S.laneIs(1.5, A), "take 1 still untouched at the end");
}

int main() {
  scenario(true, true);
  scenario(false, true);
  scenario(true, false);
  printf("\n%s\n", fails ? "SOME AUTOMATION CHECKS FAILED" : "ALL AUTOMATION CHECKS PASSED");
  return fails ? 1 : 0;
}
