// recall.h - performance recall: a recorded take plays back with exactly the sound it was performed with.
//
// The MS-20ic only sends a knob when it moves, so a MIDI recording cannot know where the other knobs were.
// The plugin therefore stores the sound itself, anchored to the notes of the take:
//  - Knobs and cables reach the plugin straight from the controller (directIn), and so do the keys
//    (directNote). The track still plays the notes, the switch and the mod wheel; knob CCs on the track
//    are ignored, so the track holds the notes and the plugin holds the sound.
//  - A note that comes from the track AND straight from the keyboard is being played live. Live playing
//    while the host runs, where no stored take is playing, is stored: a full snapshot of every setting at
//    the first note, then every change (knobs, cables, memory recalls, ESP layers, tap tempo) per engine
//    block, plus the take's notes and switch messages, all tied to the song position (PPQ).
//  - A note that comes only from the track is a region playing back. If it matches a stored note (same
//    pitch, same position), that take's settings are restored from there on and followed exactly, until
//    the take ends, the host stops or jumps. A deleted or different region has no matching notes, so it
//    simply plays with the knobs as they are.
//  - Moving a knob (or using the switch) while a stored take plays replaces its sound from that point.
//  Hosts (Logic) do not tell plugins when they record, so listening never stores anything: only playing does.
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
struct LaneMidi { double ppq; uint8_t s, d1, d2; };         // a note-on, or a switch message, of a stored take
struct LaneData {
  std::vector<LaneEv> ev;                          // sorted by ppq; every take begins with a full snapshot
  std::vector<LaneMidi> midi;                      // sorted by ppq
  std::vector<std::pair<double, double>> cov;      // stored takes [start, end] in PPQ, sorted, not overlapping
};

class Recall : public BlockHook {
public:
  Recall() { pub.store(new LaneData()); }
  ~Recall() { delete pub.load(); for (auto &r : retired) delete r.first; }

  // ======================= audio thread =======================
  // Order per host block: directNote/directIn and midiIn (the block's MIDI), then beginBlock, then process.
  //
  // The song position is counted here, sample by sample, from where the transport started: Logic reports
  // the same beat for several blocks of a live track and then catches up, so its value is only used to
  // start and to detect a real jump (more than a quarter beat away from where we are).
  void beginBlock(MS20Host *h, bool running, double hostPpq, double ppqPerSample, int nFrames) {
    host = h; pps = ppqPerSample; chunk = 0;
    const bool valid = enabled && running && hostPpq > -1e8 && ppqPerSample > 0.0;
    const bool jump = valid && (expected > -1e8) && fabs(hostPpq - expected) > 0.25;
    const double ppq = (valid && expected > -1e8 && !jump) ? expected : hostPpq;
    const int newMode = valid ? RUN : IDLE;
    if (mode == RUN && (newMode != RUN || jump)) { if (passActive) endPass(lastPpq); unlock(); }
    if (newMode == RUN) {
      LaneData *d = pub.load(std::memory_order_acquire);
      if (mode != RUN || jump || d != lane || dirty) { lane = d; rebuild(ppq); dirty = false; }
    }
    mode = newMode;
    ppq0 = ppq;
    expected = valid ? ppq + nFrames * ppqPerSample : -1e9;
    if (!passActive) lastPpq = ppq;
    clock += nFrames;
  }
  void setChunkOffset(int samples) { chunk = samples; }
  void resetAudio() { dirty = true; expected = -1e9; }
  // false = no direct connection to the controller (Windows): recall is off, the track drives everything
  void setEnabled(bool on) { enabled = on; }
  bool isEnabled() const { return enabled; }

  // a key straight from the controller (note-on with velocity > 0): marks the matching track note as live
  void directNote(uint8_t pitch) { live[liveHead] = {pitch, clock + 1}; liveHead = (liveHead + 1) % NLIVE; }

  // a knob or cable straight from the controller: always reaches the engine. While the host runs it is
  // stored with the take being played, or (over a stored take) replaces its sound from here.
  void directIn(uint8_t s, uint8_t d1, uint8_t d2) {
    if (!enabled || mode != RUN) return;
    const double x = pos();
    if (!passActive) startPass(x);
    touched = true; push(MID, x, s, packMidi(s, d1, d2));
  }

  // every MIDI message from the track. Returns false if the engine must not get it.
  bool midiIn(int offset, uint8_t s, uint8_t d1, uint8_t d2) {
    if (!enabled) return true;
    const int type = s & 0xF0;
    if (type == 0xB0 && d1 != 1) return false;                // knobs/cables: they come straight from the controller
    if (mode != RUN) return true;
    const double x = pos() + offset * pps;
    if (type == 0x90 && d2 > 0) {
      const bool isLive = takeLive(d1);
      if (passActive) { push(MID, x, 0x90, packMidi(0x90, d1, 0)); return true; }
      if (isLive) {
        if (!locked) { startPass(x); push(MID, x, 0x90, packMidi(0x90, d1, 0)); }   // playing where nothing is stored: store
        return true;                                                              // playing along over a take: no change
      }
      if (!locked && lane) { double end; if (matchNote(x, d1, end)) { locked = true; lockEnd = end; lockedA.store(true, std::memory_order_relaxed); } }
      return true;
    }
    if (type == 0xD0) {                                       // the switch: memory gestures, ESP layer, tap tempo
      if (passActive) { push(MID, x, s, packMidi(s, d1, d2)); return true; }
      if (locked && !(lane && inLog(x, s, d1, d2))) { startPass(x); touched = true; push(MID, x, s, packMidi(s, d1, d2)); }
      return true;
    }
    return true;
  }

  // per engine block, called by MS20Host::process() (k = sample index inside the current chunk)
  void before(int k) override {
    if (mode != RUN) return;
    const double x = ppq0 + (chunk + k) * pps;
    advance(x);
    if (passActive) {
      if (needSnap) {   // full snapshot at the start of the take
        for (int i = 0; i < NPARAMS; i++) { recKnown[i] = host->getParamRaw(i); push(EV, passStart, i, recKnown[i]); }
        needSnap = false;
      }
      lastPpq = x; return;
    }
    if (locked && x > lockEnd) unlock();
    if (locked) for (int i = 0; i < NPARAMS; i++)
      if (have[i] && host->getParamRaw(i) != state[i]) host->setParamNow(i, state[i]);
  }
  void after(int k) override {
    if (mode != RUN || !passActive || needSnap) return;
    const double x = ppq0 + (chunk + k) * pps;
    for (int i = 0; i < NPARAMS; i++) {
      const int32_t v = host->getParamRaw(i);
      if (v != recKnown[i]) { recKnown[i] = v; push(EV, x, i, v); }
    }
    lastPpq = x;
  }

  // any thread: a stored take is playing back (memory saves from its replayed key gestures are dropped then)
  bool takePlaying() const { return lockedA.load(std::memory_order_relaxed); }

  // ======================= main thread =======================
  void idle() {
    Msg m;
    while (q.pop(m)) {
      if (m.type == START) { passEv.clear(); passMidi.clear(); passS = m.ppq; inPass = true; overflow = false; }
      else if (m.type == EV) { if (inPass) passEv.push_back({m.ppq, m.raw, m.idx}); }
      else if (m.type == MID) { if (inPass) passMidi.push_back({m.ppq, (uint8_t)(m.raw >> 16), (uint8_t)(m.raw >> 8), (uint8_t)m.raw}); }
      else if (m.type == END) {
        bool notes = false; for (auto &n : passMidi) if ((n.s & 0xF0) == 0x90) { notes = true; break; }
        if (inPass && !overflow && notes && m.ppq - passS >= 0.02) merge(passS, m.ppq, passEv, passMidi);   // a take needs notes to be found again
        inPass = false; passEv.clear(); passMidi.clear();
      }
      else if (m.type == LOSTEVENTS) overflow = true;
    }
    for (size_t i = 0; i < retired.size();) { if (--retired[i].second <= 0) { delete retired[i].first; retired.erase(retired.begin() + i); } else i++; }
  }
  int takes() const { return (int)pub.load()->cov.size(); }
  void clear() { publish(new LaneData()); }

  std::vector<uint8_t> serialize() const {
    const LaneData *d = pub.load();
    std::vector<uint8_t> out; auto put = [&](const void *p, size_t n) { const uint8_t *b = (const uint8_t *)p; out.insert(out.end(), b, b + n); };
    const char magic[4] = {'T', 'E', 'R', '2'}; put(magic, 4);
    uint32_t np = NPARAMS, ne = (uint32_t)d->ev.size(), nm = (uint32_t)d->midi.size(), nc = (uint32_t)d->cov.size();
    put(&np, 4); put(&ne, 4); put(&nm, 4); put(&nc, 4);
    for (auto &e : d->ev) { put(&e.ppq, 8); put(&e.raw, 4); int16_t i = (int16_t)e.idx; put(&i, 2); }
    for (auto &m : d->midi) { put(&m.ppq, 8); put(&m.s, 1); put(&m.d1, 1); put(&m.d2, 1); }
    for (auto &c : d->cov) { put(&c.first, 8); put(&c.second, 8); }
    return out;
  }
  bool unserialize(const uint8_t *p, size_t n) {   // anything older than TER2 (the test builds) is ignored
    size_t at = 0; auto get = [&](void *dst, size_t k) { if (at + k > n) return false; memcpy(dst, p + at, k); at += k; return true; };
    char magic[4]; uint32_t np, ne, nm, nc;
    if (!get(magic, 4) || memcmp(magic, "TER2", 4) || !get(&np, 4) || !get(&ne, 4) || !get(&nm, 4) || !get(&nc, 4)) return false;
    if (np != (uint32_t)NPARAMS || ne > 50000000u || nm > 50000000u || nc > 1000000u) return false;
    LaneData *d = new LaneData(); d->ev.resize(ne); d->midi.resize(nm); d->cov.resize(nc);
    for (auto &e : d->ev) { int16_t i; if (!get(&e.ppq, 8) || !get(&e.raw, 4) || !get(&i, 2) || i < 0 || i >= NPARAMS) { delete d; return false; } e.idx = i; }
    for (auto &m : d->midi) if (!get(&m.ppq, 8) || !get(&m.s, 1) || !get(&m.d1, 1) || !get(&m.d2, 1)) { delete d; return false; }
    for (auto &c : d->cov) if (!get(&c.first, 8) || !get(&c.second, 8)) { delete d; return false; }
    publish(d); return true;
  }

#ifdef MS20HOST_TESTING
  const LaneData &laneForTest() const { return *pub.load(); }
  bool lockedForTest() const { return locked; }
#endif

private:
  enum { IDLE, RUN };
  enum { START, EV, MID, END, LOSTEVENTS };
  struct Msg { double ppq; int32_t raw; int16_t idx; uint8_t type; };
  struct Live { uint8_t pitch; int64_t t; };
  static const int NLIVE = 64;
  static const int64_t LIVE_WINDOW = 9600;   // a track note within ~0.2 s of the same key on the controller is live
  static int32_t packMidi(uint8_t s, uint8_t d1, uint8_t d2) { return ((int32_t)s << 16) | ((int32_t)d1 << 8) | d2; }

  // audio-thread state
  MS20Host *host = nullptr;
  bool enabled = true;
  int mode = IDLE; bool locked = false, needSnap = false, dirty = true, passActive = false, touched = false;
  std::atomic<bool> lockedA{false};
  double ppq0 = 0, pps = 0, expected = -1e9, lastPpq = 0, passStart = 0, lockEnd = 0; int chunk = 0;
  int64_t clock = 0;
  Live live[NLIVE] = {}; int liveHead = 0;
  int32_t recKnown[NPARAMS];
  LaneData *lane = nullptr; int cur = 0; int32_t state[NPARAMS]; uint8_t have[NPARAMS];
  SpscQueue<Msg, 1 << 17> q;
  bool qFull = false;

  // main-thread state
  std::atomic<LaneData *> pub{nullptr};
  std::vector<std::pair<LaneData *, int>> retired;
  std::vector<LaneEv> passEv; std::vector<LaneMidi> passMidi; double passS = 0; bool inPass = false, overflow = false;

  // song position of MIDI that arrives before beginBlock: the start of the coming block
  double pos() const { return expected > -1e8 ? expected : ppq0; }
  void push(uint8_t type, double ppq, int idx, int32_t raw) { if (!q.push({ppq, raw, (int16_t)idx, type})) qFull = true; }
  void startPass(double x) { passActive = true; passStart = x; qFull = false; touched = false; unlock(); push(START, x, 0, 0); needSnap = true; }
  void endPass(double x) {
    if (qFull) push(LOSTEVENTS, 0, 0, 0);   // events were lost: this take is not stored (better none than a wrong one)
    push(END, x, 0, 0); passActive = false; needSnap = false; touched = false;
  }
  void unlock() { locked = false; lockedA.store(false, std::memory_order_relaxed); }
  // a key from the track that was also pressed on the controller just now
  bool takeLive(uint8_t pitch) {
    for (int i = 0; i < NLIVE; i++) if (live[i].t > 0 && live[i].pitch == pitch && clock + 1 - live[i].t <= LIVE_WINDOW) { live[i].t = 0; return true; }
    return false;
  }
  // a replayed note that belongs to a stored take: returns the end of that take
  bool matchNote(double x, uint8_t pitch, double &end) const {
    const auto &c = lane->cov;
    int lo = 0, hi = (int)c.size() - 1, f = -1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (c[m].first <= x + 0.06) { f = m; lo = m + 1; } else hi = m - 1; }
    if (f < 0 || x > c[f].second + 0.06) return false;
    if (!inLog(x, 0x90, pitch, 0)) return false;
    end = c[f].second; return true;
  }
  bool inLog(double x, uint8_t s, uint8_t d1, uint8_t d2) const {   // the same message within +-0.06 beats
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
  void rebuild(double x) { for (int i = 0; i < NPARAMS; i++) have[i] = 0; cur = 0; advance(x); }

  // main thread: put one finished take [s, e] into the lane, replacing what was stored there before
  void merge(double s, double e, const std::vector<LaneEv> &nev, const std::vector<LaneMidi> &nmid) {
    if (!(e > s) || nev.empty()) return;
    const LaneData *old = pub.load();
    LaneData *d = new LaneData();
    std::vector<LaneEv> resume;   // an older take that continues after e carries on exactly as it was
    bool cont = false; for (auto &c : old->cov) if (c.first <= e && c.second > e) cont = true;
    if (cont) {
      int32_t st[NPARAMS]; uint8_t hv[NPARAMS] = {0};
      for (auto &v : old->ev) { if (v.ppq > e) break; st[v.idx] = v.raw; hv[v.idx] = 1; }
      for (int i = 0; i < NPARAMS; i++) if (hv[i]) resume.push_back({e + 1e-9, st[i], i});
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
    for (auto &c : old->cov) {
      if (c.second < s || c.first > e) { d->cov.push_back(c); continue; }
      if (c.first < s) d->cov.push_back({c.first, s - 1e-9});
      if (c.second > e) d->cov.push_back({e + 1e-9, c.second});
    }
    d->cov.push_back({s, e});
    std::sort(d->cov.begin(), d->cov.end());
    publish(d);
  }
  void publish(LaneData *d) {
    LaneData *old = pub.exchange(d, std::memory_order_acq_rel);
    if (old) retired.push_back({old, 250});
    while (retired.size() > 16) { delete retired.front().first; retired.erase(retired.begin()); }
  }
};

} // namespace ms20
