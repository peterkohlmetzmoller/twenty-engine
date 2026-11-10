// recall.h - performance recall with two engines: a recorded performance plays back exactly as it was
// played, while the instrument itself stays exactly where the panel is.
//
//  - The LIVE engine is played straight from the controller (keys, knobs, cables, switch, wheel), like the
//    hardware. It always follows the panel. Host playback never touches it.
//  - The PLAYBACK engine plays the host track (the regions). If a region's notes belong to a recorded take,
//    it follows that take's settings exactly, from wherever playback starts. Otherwise it simply uses the
//    current panel settings (it mirrors the live engine).
//  - A take is stored whenever the host's transport runs and keys are played live: a full snapshot of
//    every live-engine setting at the first note, every change after it (per 16-sample engine block), and
//    the take's notes as anchors. Hosts do not tell plugins when they record, so a jam that the host does
//    not record is stored too; its notes never appear in a region, so it is never used. Takes are kept
//    separately (the newest MAXTAKES), so a take is never changed by anything played later.
//  - Live or region: the host also sends the live playing through the track. A track message identical
//    to one that came straight from the controller a moment before is live (the live engine already has
//    it); anything else on the track is a region.
//
// Threads: the audio thread records into a lock-free queue and reads an immutable LaneData snapshot;
// the main thread builds takes, publishes new snapshots and saves/loads them with the project.
#pragma once
#include "ms20_host.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <stdint.h>
#include <string.h>

namespace ms20 {

struct LaneEv { double ppq; int32_t raw; int32_t idx; };   // a setting
struct Take {
  uint64_t id; double s, e;                         // song positions (PPQ) of the first note and the end
  std::vector<LaneEv> ev;                           // sorted by ppq; begins with a full snapshot at s
  std::vector<std::pair<double, uint8_t>> notes;    // note-ons (ppq, key), sorted
};
struct LaneData { std::vector<std::shared_ptr<const Take>> takes; };   // oldest first

class Recall {
public:
  static const int MAXTAKES = 200;

  Recall() : recHook(this), playHook(this) { pub.store(new LaneData()); }
  ~Recall() { delete pub.load(); for (auto &r : retired) delete r.first; }

  void setEngines(MS20Host *liveEngine, MS20Host *playEngine) { live = liveEngine; play = playEngine; live->setHook(&recHook); play->setHook(&playHook); }
  void setEnabled(bool on) { enabled = on; }

  // ======================= audio thread =======================
  // Per host block: directMsg (controller) and the track's MIDI first, then beginBlock, then both engines.

  // a message straight from the controller (it also goes to the live engine). Returns true if the host's
  // copy of this key already arrived (it then went to the playback engine: the caller silences it there).
  bool directMsg(uint8_t s, uint8_t d1, uint8_t d2) {
    uint8_t t, a, b; norm(s, d1, d2, t, a, b);
    bool late = false;
    for (int i = 0; i < NRECENT && t == 0x90; i++) {
      Recent &r = trackRecent[i];
      if (r.t > 0 && r.type == t && r.d1 == a && r.d2 == b && clock + 1 - r.t <= LIVE_WINDOW) { r.t = 0; late = true; break; }
    }
    if (!late) { recent[recentHead] = {t, a, b, clock + 1}; recentHead = (recentHead + 1) % NRECENT; }
    if (enabled && mode == RUN && t == 0x90) {         // a key played live while the host runs: part of a take
      const double x = pos();
      if (!passActive) startPass(x);
      push(NOTE, x, d1, 0);
    }
    return late;
  }
  // a message from the track that the controller sent a moment ago: live, already in the live engine
  bool trackIsLive(uint8_t s, uint8_t d1, uint8_t d2) {
    uint8_t t, a, b; norm(s, d1, d2, t, a, b);
    for (int i = 0; i < NRECENT; i++) {
      Recent &r = recent[i];
      if (r.t > 0 && r.type == t && r.d1 == a && r.d2 == b && clock + 1 - r.t <= LIVE_WINDOW) { r.t = 0; return true; }
    }
    trackRecent[trackHead] = {t, a, b, clock + 1}; trackHead = (trackHead + 1) % NRECENT;   // in case the controller's copy is late
    return false;
  }
  // a note-on from a region: find the take it belongs to (the playback engine follows that take)
  void trackNoteOn(int offset, uint8_t key) {
    if (!enabled || mode != RUN || !lane) return;
    const double x = pos() + offset * pps, tol = tolerance();
    const Take *found = nullptr;
    for (int i = (int)lane->takes.size() - 1; i >= 0 && !found; i--) {   // the newest take first
      const Take *t = lane->takes[i].get();
      if (x < t->s - tol || x > t->e + tol) continue;
      auto it = std::lower_bound(t->notes.begin(), t->notes.end(), x - tol, [](const std::pair<double, uint8_t> &n, double v) { return n.first < v; });
      for (; it != t->notes.end() && it->first <= x + tol; ++it) if (it->second == key) { found = t; break; }
    }
    if (found) { if (found != follow) startFollow(found, x); }
    else if (follow && (x < follow->s - tol || x > follow->e + tol)) stopFollow();   // a note outside the take: panel sound
  }

  // The song position is counted here, sample by sample, from where the transport started: Logic reports
  // the same beat for several blocks of a live track and then catches up, so its value is only used to
  // start and to detect a real jump (more than a quarter beat away from where we are).
  void beginBlock(bool running, double hostPpq, double ppqPerSample, int nFrames) {
    pps = ppqPerSample; chunk = 0;
    const bool valid = enabled && running && hostPpq > -1e8 && ppqPerSample > 0.0;
    const bool jump = valid && (expected > -1e8) && fabs(hostPpq - expected) > 0.25;
    const double ppq = (valid && expected > -1e8 && !jump) ? expected : hostPpq;
    const int newMode = valid ? RUN : IDLE;
    if (mode == RUN && (newMode != RUN || jump)) { if (passActive) endPass(lastPpq); stopFollow(); }
    LaneData *d = pub.load(std::memory_order_acquire);
    if (d != lane) {   // new takes published: keep following the same take if it is still there
      lane = d; const uint64_t id = follow ? follow->id : 0; const double fx = ppq;
      follow = nullptr; followA.store(false);
      if (id) for (auto &t : lane->takes) if (t->id == id) { startFollow(t.get(), fx); break; }
    }
    mode = newMode;
    ppq0 = ppq;
    expected = valid ? ppq + nFrames * ppqPerSample : -1e9;
    if (!passActive) lastPpq = ppq;
    clock += nFrames;
  }
  void setChunkOffset(int samples) { chunk = samples; }
  void resetAudio() { expected = -1e9; }

  // any thread: the playback engine is following a recorded take
  bool takePlaying() const { return followA.load(std::memory_order_relaxed); }

  // ======================= main thread =======================
  void idle() {
    Msg m;
    while (q.pop(m)) {
      if (m.type == START) { cur.reset(new Take()); cur->s = m.ppq; overflow = false; }
      else if (!cur) continue;
      else if (m.type == EV) cur->ev.push_back({m.ppq, m.raw, m.idx});
      else if (m.type == NOTE) cur->notes.push_back({m.ppq, (uint8_t)m.idx});
      else if (m.type == LOSTEVENTS) overflow = true;
      else if (m.type == END) {
        cur->e = m.ppq;
        if (!overflow && !cur->notes.empty() && !cur->ev.empty() && cur->e > cur->s) {
          std::stable_sort(cur->ev.begin(), cur->ev.end(), [](const LaneEv &a, const LaneEv &b) { return a.ppq < b.ppq; });
          std::sort(cur->notes.begin(), cur->notes.end());
          cur->id = ++nextId;
          LaneData *d = new LaneData(*pub.load());
          d->takes.push_back(std::shared_ptr<const Take>(cur.release()));
          while ((int)d->takes.size() > MAXTAKES) d->takes.erase(d->takes.begin());
          publish(d);
        }
        cur.reset();
      }
    }
    for (size_t i = 0; i < retired.size();) { if (--retired[i].second <= 0) { delete retired[i].first; retired.erase(retired.begin() + i); } else i++; }
  }
  int takes() const { return (int)pub.load()->takes.size(); }
  void clear() { publish(new LaneData()); }

  std::vector<uint8_t> serialize() const {
    const LaneData *d = pub.load();
    std::vector<uint8_t> out; auto put = [&](const void *p, size_t n) { const uint8_t *b = (const uint8_t *)p; out.insert(out.end(), b, b + n); };
    const char magic[4] = {'T', 'E', 'R', '3'}; put(magic, 4);
    uint32_t np = NPARAMS, nt = (uint32_t)d->takes.size(); put(&np, 4); put(&nt, 4);
    for (auto &tp : d->takes) {
      const Take &t = *tp; uint32_t ne = (uint32_t)t.ev.size(), nn = (uint32_t)t.notes.size();
      put(&t.s, 8); put(&t.e, 8); put(&ne, 4); put(&nn, 4);
      for (auto &e : t.ev) { put(&e.ppq, 8); put(&e.raw, 4); int16_t i = (int16_t)e.idx; put(&i, 2); }
      for (auto &n : t.notes) { put(&n.first, 8); put(&n.second, 1); }
    }
    return out;
  }
  bool unserialize(const uint8_t *p, size_t n) {   // anything older than TER3 (the test builds) is ignored
    size_t at = 0; auto get = [&](void *dst, size_t k) { if (at + k > n) return false; memcpy(dst, p + at, k); at += k; return true; };
    char magic[4]; uint32_t np, nt;
    if (!get(magic, 4) || memcmp(magic, "TER3", 4) || !get(&np, 4) || !get(&nt, 4)) return false;
    if (np != (uint32_t)NPARAMS || nt > (uint32_t)MAXTAKES) return false;
    LaneData *d = new LaneData();
    for (uint32_t k = 0; k < nt; k++) {
      std::unique_ptr<Take> t(new Take()); uint32_t ne, nn;
      if (!get(&t->s, 8) || !get(&t->e, 8) || !get(&ne, 4) || !get(&nn, 4) || ne > 20000000u || nn > 2000000u) { delete d; return false; }
      t->ev.resize(ne); t->notes.resize(nn);
      for (auto &e : t->ev) { int16_t i; if (!get(&e.ppq, 8) || !get(&e.raw, 4) || !get(&i, 2) || i < 0 || i >= NPARAMS) { delete d; return false; } e.idx = i; }
      for (auto &x : t->notes) if (!get(&x.first, 8) || !get(&x.second, 1)) { delete d; return false; }
      t->id = ++nextId; d->takes.push_back(std::shared_ptr<const Take>(t.release()));
    }
    publish(d); return true;
  }

#ifdef MS20HOST_TESTING
  const LaneData &laneForTest() const { return *pub.load(); }
  bool followingForTest() const { return follow != nullptr; }
#endif

private:
  enum { IDLE, RUN };
  enum { START, EV, NOTE, END, LOSTEVENTS };
  struct Msg { double ppq; int32_t raw; int16_t idx; uint8_t type; };
  struct Recent { uint8_t type, d1, d2; int64_t t; };
  static const int NRECENT = 128;
  static const int64_t LIVE_WINDOW = 16384;   // ~0.35 s: the track's copy of a live message arrives within this

  // the live engine: records takes
  struct RecHook : BlockHook {
    Recall *r; explicit RecHook(Recall *p) : r(p) {}
    void before(int k) override {
      if (r->mode != RUN || !r->passActive || !r->needSnap) return;
      for (int i = 0; i < NPARAMS; i++) { r->recKnown[i] = r->live->getParamRaw(i); r->push(EV, r->passStart, i, r->recKnown[i]); }
      r->needSnap = false;
    }
    void after(int k) override {
      if (r->mode != RUN || !r->passActive || r->needSnap) return;
      const double x = r->ppq0 + (r->chunk + k) * r->pps;
      for (int i = 0; i < NPARAMS; i++) {
        const int32_t v = r->live->getParamRaw(i);
        if (v != r->recKnown[i]) { r->recKnown[i] = v; r->push(EV, x, i, v); }
      }
      r->lastPpq = x;
    }
  } recHook;
  // the playback engine: follows a take, or mirrors the live engine (the panel)
  struct PlayHook : BlockHook {
    Recall *r; explicit PlayHook(Recall *p) : r(p) {}
    void before(int k) override {
      const int32_t *src = nullptr;
      if (r->follow) {
        const double x = r->ppq0 + (r->chunk + k) * r->pps;
        if (r->mode == RUN && x <= r->follow->e + r->tolerance()) { r->advance(x); src = r->state; }
        else r->stopFollow();
      }
      for (int i = 0; i < NPARAMS; i++) {
        const int32_t v = src ? (r->have[i] ? src[i] : r->play->getParamRaw(i)) : r->live->getParamRaw(i);
        if (r->play->getParamRaw(i) != v) r->play->setParamNow(i, v);
      }
    }
    void after(int) override {}
  } playHook;

  MS20Host *live = nullptr, *play = nullptr;
  bool enabled = true;
  int mode = IDLE; bool passActive = false, needSnap = false;
  double ppq0 = 0, pps = 0, expected = -1e9, lastPpq = 0, passStart = 0; int chunk = 0;
  int64_t clock = 0;
  Recent recent[NRECENT] = {}, trackRecent[NRECENT] = {}; int recentHead = 0, trackHead = 0;
  int32_t recKnown[NPARAMS];
  LaneData *lane = nullptr; const Take *follow = nullptr; int curEv = 0; int32_t state[NPARAMS]; uint8_t have[NPARAMS];
  std::atomic<bool> followA{false};
  SpscQueue<Msg, 1 << 17> q; bool qFull = false;

  // main-thread state
  std::atomic<LaneData *> pub{nullptr};
  std::vector<std::pair<LaneData *, int>> retired;
  std::unique_ptr<Take> cur; bool overflow = false; uint64_t nextId = 0;

  static void norm(uint8_t s, uint8_t d1, uint8_t d2, uint8_t &t, uint8_t &a, uint8_t &b) {
    t = s & 0xF0; a = d1; b = d2;
    if (t == 0x90 && d2 == 0) t = 0x80;
    if (t == 0x80) b = 0;
  }
  double pos() const { return expected > -1e8 ? expected : ppq0; }   // MIDI arrives before beginBlock
  double tolerance() const { const double t = pps * 1920.0; return t > 0.06 ? t : 0.06; }   // ~40 ms, at least 0.06 beat
  void push(uint8_t type, double ppq, int idx, int32_t raw) { if (!q.push({ppq, raw, (int16_t)idx, type})) qFull = true; }
  void startPass(double x) { passActive = true; passStart = x; qFull = false; push(START, x, 0, 0); needSnap = true; }
  void endPass(double x) { if (qFull) push(LOSTEVENTS, 0, 0, 0); push(END, x, 0, 0); passActive = false; needSnap = false; }
  void startFollow(const Take *t, double x) { follow = t; followA.store(true); for (int i = 0; i < NPARAMS; i++) have[i] = 0; curEv = 0; advance(x); }
  void stopFollow() { follow = nullptr; followA.store(false); }
  void advance(double x) {
    const auto &ev = follow->ev;
    while (curEv < (int)ev.size() && ev[curEv].ppq <= x) { state[ev[curEv].idx] = ev[curEv].raw; have[ev[curEv].idx] = 1; curEv++; }
  }
  void publish(LaneData *d) {
    LaneData *old = pub.exchange(d, std::memory_order_acq_rel);
    if (old) retired.push_back({old, 250});
    while (retired.size() > 16) { delete retired.front().first; retired.erase(retired.begin()); }
  }
};

} // namespace ms20
