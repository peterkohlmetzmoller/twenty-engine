// recall.h - performance recall: a recorded take plays back with exactly the sound it was performed with.
//
// The MS-20ic only sends a knob when it moves, so a MIDI recording cannot know where the other knobs were.
// Instead, the plugin records its own settings while the host records:
//  - when a take starts, a full snapshot of every engine setting, tied to the song position (PPQ)
//  - then every change (knobs, cables, memory recalls, ESP layers, tap tempo), per 16-sample engine block
//  - and a log of the knob/switch messages that came in, so a replay of them can be told from new playing
// Inside a recorded take the engine is held to the recorded settings at every engine block, and knob/cable
// messages from the track are ignored there. Outside takes everything is live. Starting playback in the
// middle of a take is exact too: the settings at that song position are rebuilt.
//
// Hosts (Logic) do not tell a plugin when they record, only that they play. So the rule is the same for
// playing and recording, like Latch automation built into the plugin:
//  - where nothing is stored yet, the settings are stored while the host runs (the start of every take)
//  - inside stored settings they play back exactly, until you move a control on the controller: from that
//    moment your moves replace them ("touch to overwrite"). Turning knobs while stopped never changes them.
// Knobs and cables come straight from the controller (directIn), so knob CCs on the track are always
// ignored: the track holds the notes, the plugin holds the sound. Without the direct connection (a DAW
// that holds the controller's port exclusively) the track's CCs are used, and replays are told from new
// playing by a log of the knob messages of each take.
//
// Threads: the audio thread records into a lock-free queue and reads an immutable LaneData snapshot;
// the main thread merges recordings, publishes new snapshots and saves/loads them with the project.
#pragma once
#include "ms20_host.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <stdint.h>
#include <string.h>

namespace ms20 {

struct LaneEv { double ppq; int32_t raw; int32_t idx; };   // a setting
struct LaneMidi { double ppq; uint8_t s, d1, d2; };         // a knob/switch message that came in during a take
struct LaneData {
  std::vector<LaneEv> ev;                          // sorted by ppq; every take begins with a full snapshot
  std::vector<LaneMidi> midi;                      // sorted by ppq
  std::vector<std::pair<double, double>> cov;      // recorded ranges [start, end] in PPQ, sorted, not overlapping
};

class Recall : public BlockHook {
public:
  Recall() { pub.store(new LaneData()); }
  ~Recall() { delete pub.load(); for (auto &r : retired) delete r.first; }

  // ======================= audio thread =======================
  // Once per host block, before MS20Host::process(). ppqPerSample = tempo / 60 / sampleRate.
  // The song position is counted here, sample by sample, from where the transport started: Logic reports
  // the same beat for several blocks of a live track and then catches up, so its value is only used to
  // start and to detect a real jump (more than a quarter beat away from where we are).
  void beginBlock(MS20Host *h, bool running, double hostPpq, double ppqPerSample, int nFrames) {
    host = h; pps = ppqPerSample; chunk = 0;
    // pending knob messages belong to the block they arrived just before: drop older ones
    if (staleMark > 0) { int r = 0; for (int i = staleMark; i < npend; i++) pend[r++] = pend[i]; npend = r; }
    staleMark = npend;
    const bool valid = running && hostPpq > -1e8 && ppqPerSample > 0.0;
    const bool jump = valid && (expected > -1e8) && fabs(hostPpq - expected) > 0.25;
    const double ppq = (valid && expected > -1e8 && !jump) ? expected : hostPpq;
    ppq0 = ppq;
    const int newMode = valid ? REC : IDLE;
    if (mode == REC && passActive && (newMode != REC || jump)) endPass(lastPpq);
    if (newMode == REC && (mode != REC || jump)) { touched = false; passActive = false; }
    if (newMode != IDLE) {
      LaneData *d = pub.load(std::memory_order_acquire);
      if (mode != newMode || jump || d != lane || dirty) { lane = d; rebuild(ppq); dirty = false; }
    } else { locked = false; guarded.store(false, std::memory_order_relaxed); }
    mode = newMode;
    blockStart = ppq; firstInBlock = true;
    expected = valid ? ppq + nFrames * ppqPerSample : -1e9;
    lastPpq = ppq;
  }
  // The plugin renders long host blocks in chunks: offset of the next chunk inside the host block.
  void setChunkOffset(int samples) { chunk = samples; }
  // host -> audio reset (sample rate change etc.): rebuild the playback state next block
  void resetAudio() { dirty = true; expected = -1e9; }

  // Every MIDI message from the track, before it goes to the engine (audio thread, before beginBlock of the
  // block it belongs to). Returns false if the message must be ignored.
  // directKnobs: the knobs reach the plugin straight from the controller, so knob CCs on the track are ignored.
  bool midiIn(int offset, uint8_t s, uint8_t d1, uint8_t d2, bool directKnobs) {
    const int type = s & 0xF0;
    if (directKnobs && type == 0xB0 && d1 != 1) return false;
    const bool setting = (type == 0xB0 && d1 != 1) || type == 0xD0;   // knobs, cables, the switch (CC 1 = mod wheel: playing)
    if (!setting) return true;
    // not recording yet: the coming block may start a recording, so keep it until before() knows
    if (mode != REC && npend < MAXPEND) pend[npend++] = {(double)offset, s, d1, d2};
    if (mode == IDLE) return true;
    const double x = (expected > -1e8 ? expected : ppq0) + offset * pps;   // this message's song position
    if (mode == REC) {
      if (passActive) { touched = true; push(MID, x, s, packMidi(s, d1, d2)); return true; }
      if (locked) {
        if (lane && inLog(x, s, d1, d2)) return type == 0xD0;            // the take replaying itself
        startPass(x); touched = true; locked = false;                     // the player moved something: new take
        push(MID, x, s, packMidi(s, d1, d2));
      }
      return true;
    }
    return true;
  }

  // A knob or cable message straight from the controller (audio thread, after beginBlock): always live.
  void directIn(uint8_t s, uint8_t d1, uint8_t d2) {
    if (mode != REC) return;                              // stopped: just changes the sound, stores nothing
    if (!passActive) startPass(ppq0);                     // inside stored settings: you take over from here
    touched = true; push(MID, ppq0, s, packMidi(s, d1, d2));
  }

  // per engine block, called by MS20Host::process() (k = sample index inside the current chunk)
  void before(int k) override {
    if (mode == IDLE) return;
    const double x = ppq0 + (chunk + k) * pps;
    advance(x);
    const bool cov = lane && covered(x);
    if (mode == REC) {
      if (passActive && cov && !touched) endPass(x);    // a take that only listened reaches an older take: keep that one
      if (!passActive && !cov) startPass(x);
      if (firstInBlock) {   // knob/switch messages that came in just before the recording state was known
        for (int i = 0; i < npend; i++) {
          const LaneMidi &m = pend[i]; const double px = ppq0 + m.ppq * pps;
          if (passActive) { touched = true; push(MID, px, m.s, packMidi(m.s, m.d1, m.d2)); }
          else if (!(lane && inLog(px, m.s, m.d1, m.d2))) { startPass(px); touched = true; push(MID, px, m.s, packMidi(m.s, m.d1, m.d2)); }
        }
      }
      npend = 0; staleMark = 0; firstInBlock = false;
      if (passActive) {
        if (needSnap) {   // full snapshot at the start of the take
          for (int i = 0; i < NPARAMS; i++) { recKnown[i] = host->getParamRaw(i); push(EV, passStart, i, recKnown[i]); }
          needSnap = false;
        }
        locked = false; guarded.store(false, std::memory_order_relaxed); lastPpq = x; return;
      }
    }
    if (firstInBlock) { npend = 0; staleMark = 0; firstInBlock = false; }
    locked = cov; guarded.store(cov, std::memory_order_relaxed);
    if (cov) for (int i = 0; i < NPARAMS; i++)
      if (have[i] && host->getParamRaw(i) != state[i]) host->setParamNow(i, state[i]);
  }
  void after(int k) override {
    if (mode != REC || !passActive || needSnap) return;
    const double x = ppq0 + (chunk + k) * pps;
    for (int i = 0; i < NPARAMS; i++) {
      const int32_t v = host->getParamRaw(i);
      if (v != recKnown[i]) { recKnown[i] = v; push(EV, x, i, v); }
    }
    lastPpq = x;
  }

  // diagnostics (any thread): takes started by the audio thread, takes merged by the main thread
  std::atomic<int> diagPasses{0}; int diagMerged = 0;

  // any thread: true while stored settings are playing back. Memory saves from replayed key gestures are dropped then.
  bool takePlaying() const { return guarded.load(std::memory_order_relaxed); }

  // ======================= main thread =======================
  // call regularly (e.g. every 20 ms): merges finished takes and publishes them
  void idle() {
    Msg m;
    while (q.pop(m)) {
      if (m.type == START) { passEv.clear(); passMidi.clear(); passS = m.ppq; inPass = true; overflow = false; }
      else if (m.type == EV) { if (inPass) passEv.push_back({m.ppq, m.raw, m.idx}); }
      else if (m.type == MID) { if (inPass) passMidi.push_back({m.ppq, (uint8_t)(m.raw >> 16), (uint8_t)(m.raw >> 8), (uint8_t)m.raw}); }
      else if (m.type == END) { if (inPass && !overflow && m.ppq - passS >= 0.02) merge(passS, m.ppq, passEv, passMidi); inPass = false; passEv.clear(); passMidi.clear(); }
      else if (m.type == LOSTEVENTS) overflow = true;
    }
    for (size_t i = 0; i < retired.size();) { if (--retired[i].second <= 0) { delete retired[i].first; retired.erase(retired.begin() + i); } else i++; }
  }
  int takes() const { return (int)pub.load()->cov.size(); }
  void clear() { publish(new LaneData()); }

  // save / load with the project (main thread)
  std::vector<uint8_t> serialize() const {
    const LaneData *d = pub.load();
    std::vector<uint8_t> out; auto put = [&](const void *p, size_t n) { const uint8_t *b = (const uint8_t *)p; out.insert(out.end(), b, b + n); };
    const char magic[4] = {'T', 'E', 'R', '1'}; put(magic, 4);
    uint32_t np = NPARAMS, ne = (uint32_t)d->ev.size(), nm = (uint32_t)d->midi.size(), nc = (uint32_t)d->cov.size();
    put(&np, 4); put(&ne, 4); put(&nm, 4); put(&nc, 4);
    for (auto &e : d->ev) { put(&e.ppq, 8); put(&e.raw, 4); int16_t i = (int16_t)e.idx; put(&i, 2); }
    for (auto &m : d->midi) { put(&m.ppq, 8); put(&m.s, 1); put(&m.d1, 1); put(&m.d2, 1); }
    for (auto &c : d->cov) { put(&c.first, 8); put(&c.second, 8); }
    return out;
  }
  bool unserialize(const uint8_t *p, size_t n) {
    size_t at = 0; auto get = [&](void *dst, size_t k) { if (at + k > n) return false; memcpy(dst, p + at, k); at += k; return true; };
    char magic[4]; uint32_t np, ne, nm, nc;
    if (!get(magic, 4) || memcmp(magic, "TER1", 4) || !get(&np, 4) || !get(&ne, 4) || !get(&nm, 4) || !get(&nc, 4)) return false;
    if (np != (uint32_t)NPARAMS || ne > 50000000u || nm > 50000000u || nc > 1000000u) return false;   // another engine version: ignore
    LaneData *d = new LaneData(); d->ev.resize(ne); d->midi.resize(nm); d->cov.resize(nc);
    for (auto &e : d->ev) { int16_t i; if (!get(&e.ppq, 8) || !get(&e.raw, 4) || !get(&i, 2) || i < 0 || i >= NPARAMS) { delete d; return false; } e.idx = i; }
    for (auto &m : d->midi) if (!get(&m.ppq, 8) || !get(&m.s, 1) || !get(&m.d1, 1) || !get(&m.d2, 1)) { delete d; return false; }
    for (auto &c : d->cov) if (!get(&c.first, 8) || !get(&c.second, 8)) { delete d; return false; }
    publish(d); return true;
  }

#ifdef MS20HOST_TESTING
  const LaneData &laneForTest() const { return *pub.load(); }
#endif

private:
  enum { IDLE, REC };
  enum { START, EV, MID, END, LOSTEVENTS };
  struct Msg { double ppq; int32_t raw; int16_t idx; uint8_t type; };
  static int32_t packMidi(uint8_t s, uint8_t d1, uint8_t d2) { return ((int32_t)s << 16) | ((int32_t)d1 << 8) | d2; }

  // audio-thread state
  MS20Host *host = nullptr;
  int mode = IDLE; bool locked = false, needSnap = false, dirty = true, passActive = false, touched = false;
  std::atomic<bool> guarded{false};
  double ppq0 = 0, pps = 0, expected = -1e9, lastPpq = 0, passStart = 0, blockStart = 0; int chunk = 0;
  int32_t recKnown[NPARAMS];
  LaneData *lane = nullptr; int cur = 0; int32_t state[NPARAMS]; uint8_t have[NPARAMS];
  SpscQueue<Msg, 1 << 17> q;
  bool qFull = false;
  static const int MAXPEND = 256;
  LaneMidi pend[MAXPEND]; int npend = 0, staleMark = 0; bool firstInBlock = false;   // pend[].ppq holds the sample offset

  // main-thread state
  std::atomic<LaneData *> pub{nullptr};
  std::vector<std::pair<LaneData *, int>> retired;    // freed after a few seconds (the audio thread may still read them)
  std::vector<LaneEv> passEv; std::vector<LaneMidi> passMidi; double passS = 0; bool inPass = false, overflow = false;

  void push(uint8_t type, double ppq, int idx, int32_t raw) {
    if (!q.push({ppq, raw, (int16_t)idx, type})) qFull = true;
  }
  void startPass(double x) { diagPasses.fetch_add(1, std::memory_order_relaxed); passActive = true; passStart = x; qFull = false; push(START, x, 0, 0); needSnap = true; }
  void endPass(double x) {
    if (qFull) push(LOSTEVENTS, 0, 0, 0);   // events were lost: this take is not stored (better none than a wrong one)
    push(END, x, 0, 0); passActive = false; needSnap = false; touched = false;
  }
  bool covered(double x) const {
    const auto &c = lane->cov;
    int lo = 0, hi = (int)c.size() - 1, f = -1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (c[m].first <= x) { f = m; lo = m + 1; } else hi = m - 1; }
    return f >= 0 && x <= c[f].second;
  }
  bool inLog(double x, uint8_t s, uint8_t d1, uint8_t d2) const {   // the same message within +-0.06 beats (30 ms at 120 bpm)
    const auto &m = lane->midi; const double lo = x - 0.06, hi = x + 0.06;
    auto it = std::lower_bound(m.begin(), m.end(), lo, [](const LaneMidi &a, double v) { return a.ppq < v; });
    for (; it != m.end() && it->ppq <= hi; ++it) if (it->s == s && it->d1 == d1 && it->d2 == d2) return true;
    return false;
  }
  void advance(double x) {
    if (!lane) return;
    const auto &ev = lane->ev;
    while (cur < (int)ev.size() && ev[cur].ppq <= x) { state[ev[cur].idx] = ev[cur].raw; have[ev[cur].idx] = 1; cur++; }
  }
  void rebuild(double x) {
    for (int i = 0; i < NPARAMS; i++) have[i] = 0;
    cur = 0; locked = false;
    advance(x);
  }

  // main thread: put one finished take [s, e] into the lane, replacing what was recorded there before
  void merge(double s, double e, const std::vector<LaneEv> &nev, const std::vector<LaneMidi> &nmid) {
    if (!(e > s) || nev.empty()) return;
    const LaneData *old = pub.load();
    LaneData *d = new LaneData();
    // an older take that continues after e: its settings at e, so it carries on exactly as it was
    std::vector<LaneEv> resume;
    bool cont = false; for (auto &c : old->cov) if (c.first <= e && c.second > e) cont = true;
    if (cont) {
      int32_t st[NPARAMS]; uint8_t hv[NPARAMS] = {0};
      for (auto &v : old->ev) { if (v.ppq > e) break; st[v.idx] = v.raw; hv[v.idx] = 1; }
      const double r = e + 1e-9;
      for (int i = 0; i < NPARAMS; i++) if (hv[i]) resume.push_back({r, st[i], i});
    }
    for (auto &v : old->ev) if (v.ppq < s) d->ev.push_back(v);
    for (auto &v : nev) d->ev.push_back(v);
    for (auto &v : resume) d->ev.push_back(v);
    for (auto &v : old->ev) if (v.ppq > e) d->ev.push_back(v);
    std::stable_sort(d->ev.begin(), d->ev.end(), [](const LaneEv &a, const LaneEv &b) { return a.ppq < b.ppq; });
    for (auto &m : old->midi) if (m.ppq < s) d->midi.push_back(m);
    for (auto &m : nmid) d->midi.push_back(m);
    for (auto &m : old->midi) if (m.ppq > e) d->midi.push_back(m);
    std::stable_sort(d->midi.begin(), d->midi.end(), [](const LaneMidi &a, const LaneMidi &b) { return a.ppq < b.ppq; });
    for (auto &c : old->cov) {   // old ranges minus [s, e]
      if (c.second < s || c.first > e) { d->cov.push_back(c); continue; }
      if (c.first < s) d->cov.push_back({c.first, s - 1e-9});
      if (c.second > e) d->cov.push_back({e + 1e-9, c.second});
    }
    d->cov.push_back({s, e});
    std::sort(d->cov.begin(), d->cov.end());
    publish(d); diagMerged++;
  }
  void publish(LaneData *d) {
    LaneData *old = pub.exchange(d, std::memory_order_acq_rel);
    if (old) retired.push_back({old, 250});   // ~5 s of idle calls
    while (retired.size() > 16) { delete retired.front().first; retired.erase(retired.begin()); }   // the audio thread only holds the newest
  }
};

} // namespace ms20
