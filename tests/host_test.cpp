// host_test.cpp - checks the plugin host layer on a computer (no plugin framework needed)
#include "../engine/ms20_host.h"
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
struct Ev { double t; uint8_t s, d1, d2; };
static std::vector<Ev> song() {
  std::vector<Ev> v; auto on=[&](double t,int n){v.push_back({t,0x90,(uint8_t)n,100});}; auto off=[&](double t,int n){v.push_back({t,0x80,(uint8_t)n,0});};
  on(0.1,48);on(0.1,55);on(0.1,64);off(1.2,48);off(1.2,55);off(1.2,64);v.push_back({0.5,0xB0,74,20});v.push_back({0.9,0xB0,74,100});
  on(1.5,72);off(2.0,72); return v; }
// render the song through MS20Host at a host rate with given block sizes (blk<0: random 1..1024)
static bool g_noclick = false;
static std::vector<float> render(double sr, int blk, double secs, unsigned seed = 1) {
  MS20Host *h = new MS20Host(); h->reset(sr); auto ev = song();
  if (g_noclick) h->setParamRaw(PARAM_INDEX_Filter__Macro_clicknoise, 0);
  long long N = (long long)(secs * sr), done = 0; std::vector<float> out; out.reserve(N * 2); srand(seed);
  std::vector<float> L(4096), R(4096);
  while (done < N) {
    int n = blk > 0 ? blk : 1 + rand() % 1024; if (done + n > N) n = (int)(N - done);
    for (auto &e : ev) { long long s = (long long)llround(e.t * sr); if (s >= done && s < done + n) h->midi((int)(s - done), e.s, e.d1, e.d2); }
    h->process(L.data(), R.data(), n);
    for (int i = 0; i < n; i++) { out.push_back(L[i]); out.push_back(R[i]); }
    ParamSet p; while (h->popEngineChange(p)) {} LedMsg m; while (h->popLed(m)) {}
    done += n;
  }
  delete h; return out;
}
static std::vector<float> renderCore(double secs) {   // the engine directly, 48 kHz
  EngineCore *c = new EngineCore(); c->init(); auto ev = song(); if (g_noclick) c->root.PExch[PARAM_INDEX_Filter__Macro_clicknoise].value = 0; std::vector<float> out;
  int nb = (int)(secs * 48000 / 16);
  for (int b = 0; b < nb; b++) { for (auto &e : ev) { long long s = llround(e.t * 48000) + 32; if (s >= b * 16 && s < b * 16 + 16) c->midi(e.s, e.d1, e.d2); }   /* same fixed 32-sample MIDI delay */
    c->process(); for (int i = 0; i < 16; i++) { out.push_back(c->audioOutL[i] / 134217728.f); out.push_back(c->audioOutR[i] / 134217728.f); } }
  delete c; return out; }
static double maxdiff(const std::vector<float> &a, const std::vector<float> &b) { double m = 0; size_t n = a.size() < b.size() ? a.size() : b.size(); for (size_t i = 0; i < n; i++) m = fmax(m, fabs(a[i] - b[i])); return m; }
static double freq(const std::vector<float> &x, double sr, double t0, double t1) {   // zero crossings of left channel
  int z = 0; long a = (long)(t0 * sr), b = (long)(t1 * sr); for (long i = a + 1; i < b; i++) if (x[2*(i-1)] < 0 && x[2*i] >= 0) z++; return z / (t1 - t0); }
int main() {
  printf("1. 48 kHz: identical to the engine itself \n");
  auto a = render(48000, 512, 2.5), c = renderCore(2.5);
  printf("     max difference %.2e\n", maxdiff(a, c)); CHECK(maxdiff(a, c) == 0.0, "bit-identical");
  printf("2. Block size does not matter\n");
  auto b1 = render(44100, 64, 2.5), b2 = render(44100, -1, 2.5, 7), b3 = render(44100, 1, 2.5);
  printf("     44.1 kHz, 64 vs random vs 1-sample blocks: max diff %.2e / %.2e\n", maxdiff(b1, b2), maxdiff(b1, b3));
  CHECK(maxdiff(b1, b2) == 0.0 && maxdiff(b1, b3) == 0.0, "same output whatever the host block size");
  printf("3. Other sample rates keep the pitch\n");
  double f48 = freq(a, 48000, 1.55, 1.95);
  for (double sr : {44100.0, 88200.0, 96000.0}) { auto x = render(sr, 256, 2.5); double f = freq(x, sr, 1.55, 1.95);
    printf("     %6.0f Hz: C5 note at %.1f Hz (48 kHz: %.1f Hz)\n", sr, f, f48); CHECK(fabs(1200 * log2(f / f48)) < 3, "pitch within 3 cents"); }
  printf("4. Parameters both ways\n");
  { MS20Host h; h.reset(44100); std::vector<float> L(512), R(512);
    h.midi(0, 0xB0, 74, 64); h.process(L.data(), R.data(), 512); ParamSet p; bool got = false; int32_t v = 0;
    while (h.popEngineChange(p)) if (p.idx == PARAM_INDEX_Filter__Macro_lpfcut) { got = true; v = p.raw; }
    CHECK(got && v == 0, "CC74 from the iC is reported to the host (LPF cutoff = 0, centre)");
    h.setParamRaw(PARAM_INDEX_Filter__Macro_lpfcut, 20 << 21); h.process(L.data(), R.data(), 512);
    CHECK(h.getParamRaw(PARAM_INDEX_Filter__Macro_lpfcut) == (20 << 21), "host automation reaches the engine");
    bool echo = false; while (h.popEngineChange(p)) if (p.idx == PARAM_INDEX_Filter__Macro_lpfcut) echo = true;
    CHECK(!echo, "a host change is not echoed back");
    LedMsg m; int nl = 0; for (int i = 0; i < 100; i++) h.process(L.data(), R.data(), 512); while (h.popLed(m)) nl++;
    printf("     LED messages queued in ~1.2 s: %d\n", nl); CHECK(nl > 0, "LED SysEx reaches the host queue");
    h.reset(48000); CHECK(h.getParamRaw(PARAM_INDEX_Filter__Macro_lpfcut) == (20 << 21), "settings survive a sample-rate change"); }
  printf("5. Resampler quality (44.1 kHz render vs ideal resampling of the 48 kHz render)\n");
  { auto x = render(44100, 256, 2.5); FILE *f = fopen("q44.raw", "wb"); fwrite(x.data(), 4, x.size(), f); fclose(f);
    f = fopen("q48.raw", "wb"); fwrite(a.data(), 4, a.size(), f); fclose(f); printf("     written q44.raw / q48.raw for analysis\n"); }
  printf("6. Speed\n");
  { MS20Host h; h.reset(44100); std::vector<float> L(512), R(512); for (int n : {48, 52, 55, 60, 64, 67, 72, 76}) h.midi(0, 0x90, n, 100);
    clock_t t = clock(); for (int i = 0; i < 862; i++) h.process(L.data(), R.data(), 512); double s = double(clock() - t) / CLOCKS_PER_SEC;
    printf("     10 s of 8-voice chord at 44.1 kHz in %.2f s (%.0fx real time, one core)\n", s, 10 / s); }
  printf("\n%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "ALL HOST CHECKS PASSED", fails); return fails;
}
