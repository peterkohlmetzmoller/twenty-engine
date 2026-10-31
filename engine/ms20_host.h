// ms20_host.h - runs the generated MS-20 engine inside any host (plugin, app, test):
//  - the engine always runs at 48 kHz in 16-sample blocks (exactly as on the Ksoloti);
//    output is converted to the host sample rate with a windowed-sinc resampler (bypassed at 48 kHz)
//  - timestamped MIDI in, applied at 16-sample (0.33 ms) resolution
//  - parameters: host -> engine through a lock-free queue (applied on the audio thread);
//    engine -> host (CC from the controller, knob layers, tap tempo, patch bay) reported through another queue
//  - LED SysEx for the controller collected in a queue the host sends from its main thread
// Realtime safe: no allocation, locks or system calls in process().
#pragma once
#include "engine_gen.h"
#include <atomic>
#include <math.h>

namespace ms20 {

// single-producer / single-consumer ring buffer (one thread pushes, one thread pops)
template <typename T, int N>
struct SpscQueue {
  T buf[N];
  std::atomic<int> head{0}, tail{0};
  bool push(const T &v) {
    int h = head.load(std::memory_order_relaxed), n = (h + 1) % N;
    if (n == tail.load(std::memory_order_acquire)) return false;
    buf[h] = v; head.store(n, std::memory_order_release); return true;
  }
  bool pop(T &v) {
    int t = tail.load(std::memory_order_relaxed);
    if (t == head.load(std::memory_order_acquire)) return false;
    v = buf[t]; tail.store((t + 1) % N, std::memory_order_release); return true;
  }
};

struct ParamSet { int idx; int32_t raw; };
struct LedMsg { uint8_t b[16]; int len; };
struct MidiEv { double te; uint8_t s, d1, d2; };   // te: engine sample time

class MS20Host {
public:
  static const int ENGINE_SR = 48000;

  MS20Host() { core = new EngineCore(); reset(48000.0); }
  ~MS20Host() { delete core; }

  // call when the sample rate changes or the plugin is reset (not realtime)
  void reset(double hostRate) {
    int32_t keep[NPARAMS]; bool had = inited;
    if (had) { for (int i = 0; i < NPARAMS; i++) keep[i] = core->root.PExch[i].value; saveMemTable(); }
    core->init();
    if (had) { for (int i = 0; i < NPARAMS; i++) core->root.PExch[i].value = keep[i]; restoreMemTable(); }
    for (int i = 0; i < NPARAMS; i++) known[i] = core->root.PExch[i].value;
    inited = true;
    rate = hostRate; step = ENGINE_SR / hostRate;
    bypass = fabs(hostRate - ENGINE_SR) < 0.5;
    // anti-aliasing cutoff relative to the engine's Nyquist: below the lower of the two Nyquists
    double fc = (hostRate < ENGINE_SR) ? (0.5 * hostRate - 1500.0) / (0.5 * ENGINE_SR) : 0.94;   // 44.1k: 20.55 kHz
    buildKernel(fc);
    for (int i = 0; i < RB; i++) ebL[i] = ebR[i] = 0.f;
    produced = 0; pos = bypass ? 0.0 : (double)HW;
    evlat = 2 * BUFSIZE + (bypass ? 0 : HW);
    nev = 0;
  }

  // --- MIDI (audio thread, before process(); offset = sample position in the coming host block) ---
  // Events are scheduled a fixed EVLAT engine samples ahead, so they always land in a block that has not been
  // rendered yet: the result is identical whatever block sizes the host uses (0.7-1 ms constant delay).
  void midi(int offset, uint8_t s, uint8_t d1, uint8_t d2) {
    if (nev < MAXEV) { MidiEv e = {pos + offset * step + evlat, s, d1, d2}; int j = nev++; while (j > 0 && ev[j - 1].te > e.te) { ev[j] = ev[j - 1]; j--; } ev[j] = e; }
  }

  // --- parameters ---
  // host/UI thread: queue a value for the engine (raw Ksoloti units)
  void setParamRaw(int idx, int32_t raw) { if (idx >= 0 && idx < NPARAMS) toEngine.push({idx, raw}); }
  // host/UI thread: changes the engine made by itself since the last call
  bool popEngineChange(ParamSet &p) { return fromEngine.pop(p); }
  // host/UI thread: LED SysEx to send to the controller
  bool popLed(LedMsg &m) { return leds.pop(m); }
  // read the current value (any thread; a plain aligned 32-bit read)
  int32_t getParamRaw(int idx) const { return core->root.PExch[idx].value; }

  // --- audio (audio thread) ---
  void process(float *outL, float *outR, int n) {
    ParamSet ps;
    while (toEngine.pop(ps)) { core->root.PExch[ps.idx].value = ps.raw; known[ps.idx] = ps.raw; }
    g_sysexSender = &MS20Host::onSysex; g_sysexUser = this;
    int ei = 0;
    for (int k = 0; k < n; k++) {
      double need = bypass ? pos : pos + HW;
      while ((double)produced <= need) {
        // apply every event whose time falls before the end of the next engine block
        while (ei < nev && ev[ei].te < (double)(produced + BUFSIZE)) {
          core->midi(ev[ei].s, ev[ei].d1, ev[ei].d2); ei++;
        }
        core->process();
        for (int i = 0; i < BUFSIZE; i++) {
          int w = (int)((produced + i) & (RB - 1));
          ebL[w] = core->audioOutL[i] * (1.f / 134217728.f);
          ebR[w] = core->audioOutR[i] * (1.f / 134217728.f);
        }
        produced += BUFSIZE;
      }
      if (bypass) {
        int r = (int)((long long)pos & (RB - 1)); outL[k] = ebL[r]; outR[k] = ebR[r];
      } else {
        interp(pos, outL[k], outR[k]);
      }
      pos += step;
    }
    // keep events that belong to later blocks
    int r = 0; for (int i = ei; i < nev; i++) ev[r++] = ev[i]; nev = r;
    // report changes the engine made itself
    for (int i = 0; i < NPARAMS; i++) {
      int32_t v = core->root.PExch[i].value;
      if (v != known[i]) { if (fromEngine.push({i, v})) known[i] = v; }
    }
    g_sysexSender = 0; g_sysexUser = 0;
  }

#ifdef MS20HOST_TESTING
  // test hook: run the resampler over any 48 kHz signal (mono)
  int resampleTest(const float *in, int nin, float *out, int maxout) {
    for (int i = 0; i < RB; i++) ebL[i] = ebR[i] = 0.f;
    int no = 0; double p = HW;
    while (no < maxout) { long long need = (long long)p + HW; if (need >= nin) break;
      for (long long i = need - 2 * HW - 2; i <= need; i++) if (i >= 0) { ebL[i & (RB - 1)] = in[i]; ebR[i & (RB - 1)] = in[i]; }
      float l, r; interp(p, l, r); out[no++] = l; p += step; }
    return no;
  }
#endif
  double latencySamples() const { return (evlat) / step; }   // MIDI-to-audio delay in host samples

  // --- patch memory (36 sounds on the keys) ---
  // main thread, once after construction: folder for MS20MEM.BIN (ending in '/'); loads it if it exists
  bool loadMemories(const char *folder) {
    snprintf(g_memDir, sizeof(g_memDir), "%s", folder);
    return core->o_Patch_Memory.loadFile();
  }
  // main thread, regularly (e.g. every 20 ms): writes the file after a save on the keys
  void pollMemorySave() {
    if (core->o_Patch_Memory.savereq) { core->o_Patch_Memory.savereq = 0;
      if (core->o_Patch_Memory.saveFile()) core->o_Patch_Memory.flash(31, 5); else core->o_Patch_Memory.flash(12, 8); }   // saved: 5 x all LEDs, failed: 8 x PEAK+TRIG
  }

private:
  static const int HW = 40;           // resampler half width in engine samples (80 taps, about 0.8 ms)
  static const int PH = 512;          // kernel phases
  static const int RB = 4096;         // engine output ring (power of two)
  static const int MAXEV = 1024;

  EngineCore *core = nullptr;
  bool inited = false;
  int32_t known[NPARAMS];
  SpscQueue<ParamSet, 1024> toEngine, fromEngine;
  SpscQueue<LedMsg, 256> leds;
  MidiEv ev[MAXEV]; int nev = 0;

  double rate = 48000, step = 1, pos = 0, evlat = 32; bool bypass = true;
  long long produced = 0;
  float ebL[RB], ebR[RB];
  float kern[(PH + 1) * 2 * HW];

  // the memory table survives an engine reset (sample-rate change) like the parameters do
  int32_t memKeep[sizeof(((O_Patch_Memory *)0)->_mem) / sizeof(int32_t)]; uint8_t validKeep[sizeof(((O_Patch_Memory *)0)->_valid)];
  void saveMemTable() { memcpy(memKeep, core->o_Patch_Memory._mem, sizeof(memKeep)); memcpy(validKeep, core->o_Patch_Memory._valid, sizeof(validKeep)); }
  void restoreMemTable() { memcpy(core->o_Patch_Memory._mem, memKeep, sizeof(memKeep)); memcpy(core->o_Patch_Memory._valid, validKeep, sizeof(validKeep)); }

  static void onSysex(const uint8_t *b, int len, void *user) {
    MS20Host *h = (MS20Host *)user; LedMsg m; m.len = len > 16 ? 16 : len;
    memcpy(m.b, b, m.len); h->leds.push(m);
  }
  static double bessI0(double x) { double s = 1, t = 1; for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
  void buildKernel(double fc) {
    const double beta = 8.0, i0b = bessI0(beta);
    for (int p = 0; p <= PH; p++) {
      double frac = (double)p / PH; double sum = 0;
      for (int j = 0; j < 2 * HW; j++) {
        double x = (j - HW + 1) - frac;                         // tap distance in engine samples
        double sinc = (fabs(x) < 1e-9) ? 1.0 : sin(M_PI * fc * x) / (M_PI * fc * x);
        double r = x / HW; double w = (fabs(r) >= 1.0) ? 0.0 : bessI0(beta * sqrt(1 - r * r)) / i0b;
        double v = fc * sinc * w; kern[p * 2 * HW + j] = (float)v; sum += v;
      }
      for (int j = 0; j < 2 * HW; j++) kern[p * 2 * HW + j] /= (float)sum;   // unity gain at DC
    }
  }
  void interp(double p, float &l, float &r) {
    long long ip = (long long)floor(p); double fr = p - ip;
    double fp = fr * PH; int pi = (int)fp; float t = (float)(fp - pi);
    const float *k0 = &kern[pi * 2 * HW], *k1 = &kern[(pi + 1) * 2 * HW];
    float al = 0, ar = 0;
    for (int j = 0; j < 2 * HW; j++) {
      float c = k0[j] + t * (k1[j] - k0[j]);
      int w = (int)((ip - HW + 1 + j) & (RB - 1));
      al += c * ebL[w]; ar += c * ebR[w];
    }
    l = al; r = ar;
  }
};

} // namespace ms20
