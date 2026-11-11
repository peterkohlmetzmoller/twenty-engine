// paramstream.h - Twenty Engine Automation: the sound carried in the region as MIDI.
//
// The plugin sends every setting of its live engine (the panel) through its own MIDI port ("Twenty Engine
// Panel", led_port.h PanelPort). A recording track whose input is "All" records it into the region next to
// the notes, so the sound travels with the region, can be edited as MIDI, and needs no DAW automation:
//   - when the transport starts: the whole panel (a recording starts with its sound; with count-in it lands
//     in the count-in bar),
//   - after that: every change (knob, switch, memory recall, cable), as it happens.
// Playing the region back, the playback engine takes its settings from this stream; the MS-20ic's own knob
// CCs in the region (channel 1) are ignored. Starting in the middle of a region works through the DAW's chase
// (it resends the last value of every controller), because each setting has its own controller number.
//
// Encoding: one controller number per setting, 14 bits: MIDI channel 16 = high 7 bits, channel 15 = low 7.
// Two extra slots carry the mono cable (KBD CV -> VCO 1+2 CV IN) and the tap-tempo cable, which are cable
// events in the engine rather than settings: high byte 127 = patched, 0 = not.
//
// Echo: while the DAW runs, it passes the track's live input back to the plugin, so the plugin also receives
// what it just sent - which can look exactly like the region (playing back a take just recorded: the panel
// still has its sound, and the region's snapshot and the plugin's own arrive at the same moment). So every
// whole-panel snapshot starts with a snapshot number (a new random one each time):
//   - the plugin's own current number coming back: the burst behind it is its echo, ignored;
//   - any other number (recorded in the region): the burst behind it is applied, whatever the values.
// Single changes (knobs turned while listening) have no number: a value equal to one the plugin sent within
// ECHO_MS is ignored once that setting has received something since playback started. The caller clears that
// at every stop (stoppedBlock), except in a block that delivered stream data (the DAW's chase can arrive just
// before it reports that it runs).
#pragma once
#include <stdint.h>
#include <string.h>
#include <atomic>
#include <chrono>

namespace ms20 {

template <int NP>
struct ParamStream {
  static const int NSLOT = NP + 2;                  // the settings, then mono and tap
  static const int SLOT_MONO = NP, SLOT_TAP = NP + 1;
  static const uint8_t CH_HI = 15, CH_LO = 14;      // MIDI channels 16 and 15 (0-based)
  static const int ECHO_MS = 150;
  static const int MARK_CC = 50;                    // the snapshot number (14 bits, same two channels)
  static const int SNAP_EVERY_MS = 1000;
  static const int SNAP_MSGS = 2 * NP + 2;          // the values in one whole-panel snapshot (after its number)            // while the transport runs, the whole panel again every second,
                                                    // so every piece of a split region carries its sound

  enum Kind : uint8_t { UNI, BI, INTV };
  Kind kind[NSLOT];

  // controller numbers: 1-5, 7-31, 70-95, 102-119 (no bank select, data entry, pedals, NRPN/RPN or mode messages)
  static int CcOf(int slot) {
    if (slot < 0 || slot >= NSLOT) return -1;
    if (slot < 30) return slot < 5 ? slot + 1 : slot + 2;   // 1-5, 7-31
    if (slot < 56) return 70 + (slot - 30);                  // 70-95
    return slot - 56 < 18 ? 102 + (slot - 56) : -1;          // 102-119
  }
  static int SlotOf(int cc) {
    int s = -1;
    if (cc >= 1 && cc <= 5) s = cc - 1;
    else if (cc >= 7 && cc <= 31) s = cc - 2;
    else if (cc >= 70 && cc <= 95) s = 30 + (cc - 70);
    else if (cc >= 102 && cc <= 119) s = 56 + (cc - 102);
    return (s >= 0 && s < NSLOT) ? s : -1;
  }
  static_assert(NSLOT <= 74, "more settings than controller numbers");

  // raw engine units <-> 14 bits
  static const int32_t FULL = 64 << 21;             // 64 knob units
  int v14(int slot, int32_t raw) const {
    double v;
    switch (kind[slot]) {
      case UNI: v = (double)raw * 16383.0 / FULL; break;
      case BI:  v = ((double)raw + FULL) * 16383.0 / (2.0 * FULL); break;
      default:  v = raw; break;
    }
    int r = (int)(v + (v >= 0 ? 0.5 : -0.5));
    return r < 0 ? 0 : r > 16383 ? 16383 : r;
  }
  int32_t raw(int slot, int v) const {
    switch (kind[slot]) {
      case UNI: return (int32_t)((double)v * FULL / 16383.0 + 0.5);
      case BI:  { const double r = (double)v * 2.0 * FULL / 16383.0 - FULL; return (int32_t)(r + (r >= 0 ? 0.5 : -0.5)); }
      default:  return v;
    }
  }

  // ---- sending (main thread) ----
  // send(status, cc, value) for one setting: low byte first, then the high byte (the high byte completes it)
  template <class F> void encode(int slot, int32_t rawValue, F send) {
    const int v = (slot >= NP) ? (rawValue ? 16383 : 0) : v14(slot, rawValue);
    const int cc = CcOf(slot), lo = v & 127, hi = v >> 7;
    const int64_t now = NowMs();
    if (slot < NP) { sentLo[slot].store(lo); sentLoT[slot].store(now); send((uint8_t)(0xB0 | CH_LO), (uint8_t)cc, (uint8_t)lo); }
    sentHi[slot].store(hi); sentHiT[slot].store(now); send((uint8_t)(0xB0 | CH_HI), (uint8_t)cc, (uint8_t)hi);
  }

  // the snapshot number, before a whole-panel snapshot (main thread)
  template <class F> void encodeMark(F send) {
    uint32_t r = (uint32_t)NowMs() * 2654435761u ^ (uint32_t)(++markSeq * 40503u);
    int n = (int)(r % 16383) + 1;
    if (n == myMark.load()) n = n % 16383 + 1;
    prevMark.store(myMark.load()); myMark.store(n);
    send((uint8_t)(0xB0 | CH_LO), (uint8_t)MARK_CC, (uint8_t)(n & 127));
    send((uint8_t)(0xB0 | CH_HI), (uint8_t)MARK_CC, (uint8_t)(n >> 7));
  }

  // ---- receiving (audio thread) ----
  // push(): a message from the track; true if it belongs to the stream (kept for flush, the caller drops it).
  // flush(apply) once per audio block, before the engines run: decides per block, whatever the order of the
  // messages inside it, and calls apply(slot, raw) for every value that is not the plugin's own echo.
  bool push(uint8_t st, uint8_t cc, uint8_t val) {
    if ((st & 0xF0) != 0xB0) return false;
    const uint8_t ch = st & 0x0F;
    if (ch != CH_HI && ch != CH_LO) return false;
    if (nq < QCAP) q[nq++] = {ch, cc, val};
    return true;
  }
  int lastN = 0, lastApplied = 0; bool lastOwn = false, lastForeign = false;   // diagnostics: the last flush
  template <class F> void flush(F apply) {
    sawData = nq > 0;
    lastN = nq; lastApplied = 0; lastOwn = lastForeign = false;
    if (!nq) return;
    const int64_t now = NowMs();
    // 1. snapshot numbers in this block: whose snapshot is it?
    bool own = false, foreign = false; int mlo = markLo;
    for (int k = 0; k < nq; k++) if (q[k].cc == MARK_CC && q[k].ch == CH_LO) mlo = q[k].val;
    for (int k = 0; k < nq; k++) if (q[k].cc == MARK_CC && q[k].ch == CH_HI) {
      const int n = (q[k].val << 7) | mlo;
      if (n == myMark.load() || n == prevMark.load()) own = true; else foreign = true;
    }
    markLo = mlo; lastOwn = own; lastForeign = foreign;
    // our own snapshot coming back (echo): the whole burst is ignored - also the part of it that the host
    // delivers in the next block(s), where its number is not; a region's snapshot in the same block wins
    int ownSkip = 0;
    if (own && !foreign) {
      int first = 0; while (first < nq && q[first].cc != MARK_CC) first++;
      ownLeft = SNAP_MSGS; for (int k = first; k < nq; k++) if (q[k].cc != MARK_CC) ownLeft--;
      if (ownLeft < 0) ownLeft = 0;
      nq = 0; return;
    }
    if (!own && !foreign && ownLeft > 0) { ownSkip = ownLeft < nq ? ownLeft : nq; ownLeft -= ownSkip; }
    if (foreign) ownLeft = 0;
    // 2. the values: low bytes before high bytes (a high byte completes a setting); with a foreign snapshot in
    //    the block, values equal to ours go first and the others (the region's) last, so the region wins
    for (int pass = 0; pass < 4; pass++) {
      const bool wantHi = pass & 1, wantMine = pass < 2;
      for (int k = ownSkip; k < nq; k++) {
        const Msg &m = q[k];
        if (m.cc == MARK_CC || (m.ch == CH_HI) != wantHi) continue;
        const int slot = SlotOf(m.cc);
        if (slot < 0 || (m.ch == CH_LO && slot >= NP)) continue;
        const bool mine = (m.ch == CH_HI ? sentHi[slot].load() : sentLo[slot].load()) == m.val &&
                          now - (m.ch == CH_HI ? sentHiT[slot].load() : sentLoT[slot].load()) <= ECHO_MS + 50;
        if (mine != wantMine) continue;
        bool use;
        if (foreign) use = true;                                     // a recorded snapshot: everything applies
        else use = !(mine && (m.ch == CH_HI ? gotHi[slot] : gotLo[slot]));   // single changes: the echo rule
        if (!use) continue;
        lastApplied++;
        if (m.ch == CH_LO) { gotLo[slot] = true; lo[slot] = m.val; if (hi[slot] >= 0) apply(slot, raw(slot, (hi[slot] << 7) | lo[slot])); }
        else {
          gotHi[slot] = true; hi[slot] = m.val;
          if (slot >= NP) apply(slot, m.val >= 64 ? 1 : 0); else apply(slot, raw(slot, (m.val << 7) | lo[slot]));
        }
      }
    }
    nq = 0;
  }
  void resetDecoder() { for (int s = 0; s < NSLOT; s++) { hi[s] = -1; lo[s] = 0; } ownLeft = 0; resetRun(); }
  // audio thread, once per block while the transport is stopped: a new playback starts fresh
  // (skipped in a block that delivered stream data: that may be the chase of the coming start)
  void stoppedBlock() { if (!sawData) { resetRun(); ownLeft = 0; } sawData = false; }   // after flush()
  void runningBlock() { sawData = false; }
  void resetRun() { for (int s = 0; s < NSLOT; s++) { gotHi[s] = gotLo[s] = false; } }

  ParamStream() {
    for (int s = 0; s < NSLOT; s++) { kind[s] = INTV; hi[s] = -1; lo[s] = 0; sentHi[s] = -1; sentLo[s] = -1; sentHiT[s] = -1000000; sentLoT[s] = -1000000; }
  }
#ifdef PARAMSTREAM_SIM_CLOCK
  static int64_t NowMs() { return SimMs(); }
  static int64_t& SimMs() { static int64_t t = 1000000; return t; }   // tests: simulated time
#else
  static int64_t NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
#endif

private:
  int hi[NSLOT], lo[NSLOT];
  bool gotHi[NSLOT] = {}, gotLo[NSLOT] = {}, sawData = false;
  int markLo = 0, ownLeft = 0;
  struct Msg { uint8_t ch, cc, val; };
  static const int QCAP = 1024;
  Msg q[QCAP]; int nq = 0;
  std::atomic<int> myMark{-1}, prevMark{-1}; uint32_t markSeq = 0;
  std::atomic<int> sentHi[NSLOT], sentLo[NSLOT];
  std::atomic<int64_t> sentHiT[NSLOT], sentLoT[NSLOT];
};

// StreamMemory: the region's sound by song position, as heard while the regions played.
// The host neither resends the stream when playback starts inside a region (no chase in Logic) nor keeps the
// plugin's state across a start (it resets the plugin). So every whole-panel snapshot from a region is kept
// with its song position; starting or jumping somewhere, the playback engine takes the snapshot that belongs
// there at once. Played through again, what lay between two snapshots is renewed (a region moved or edited).
template <int NV>
struct StreamMemory {
  static const int CAP = 1024;
  static constexpr double SAME = 0.02;               // the same snapshot position (song position units)
  struct Snap { double pos; int32_t v[NV]; };
  Snap s[CAP]; int n = 0;
  double last = 0.0; bool cont = false;

  // a region's snapshot heard at song position pos (the transport running since the previous one: anything
  // remembered between the two is gone)
  void store(double pos, const int32_t *v) {
    if (cont && pos > last + SAME) {
      int w = 0;
      for (int r = 0; r < n; r++) if (!(s[r].pos > last + SAME && s[r].pos < pos - SAME)) { if (w != r) s[w] = s[r]; w++; }
      n = w;
    }
    int k = 0; while (k < n && s[k].pos < pos - SAME) k++;
    if (k < n && s[k].pos <= pos + SAME) { s[k].pos = pos; memcpy(s[k].v, v, sizeof(s[k].v)); }
    else if (n < CAP) { for (int r = n; r > k; r--) s[r] = s[r - 1]; s[k].pos = pos; memcpy(s[k].v, v, sizeof(s[k].v)); n++; }
    last = pos; cont = true;
  }
  void breakRun() { cont = false; }                  // stopped or jumped: the next snapshot starts a new stretch
  // the sound at song position pos: a snapshot just after it (within ahead: a take recorded without count-in
  // has its first snapshot a moment after its first notes), else the last one before it; null if none
  const int32_t *at(double pos, double ahead) const {
    const Snap *best = nullptr;
    for (int k = 0; k < n; k++) { if (s[k].pos <= pos + ahead) best = &s[k]; else break; }
    return best ? best->v : nullptr;
  }
  void clear() { n = 0; cont = false; }
};

}  // namespace ms20
