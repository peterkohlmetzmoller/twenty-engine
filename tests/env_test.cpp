// env_test.cpp - 1.1.1: how fast the envelopes are at the knob positions (attack / decay with the VCA)
#include ENGINE_HOST
#include <stdio.h>
#include <math.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;
static float bl[48000], br[48000];
static void run(MS20Host &h, int n) { int o = 0; while (n > 0) { int k = n > 512 ? 512 : n; h.process(bl + o, br + o, k); n -= k; o += k; } }
int main() {
  const float pos[] = {0.f, 0.125f, 0.25f, 0.5f};
  for (float p : pos) {
    MS20Host h; h.reset(48000); run(h, 4800);
    int32_t k = (int32_t)(p * 134217727.f);
    h.setParamRaw(PARAM_INDEX_EG2_hold, 0); h.setParamRaw(PARAM_INDEX_EG2_attack, k); h.setParamRaw(PARAM_INDEX_EG2_decay, k);
    h.setParamRaw(PARAM_INDEX_EG2_sustain, 0); h.setParamRaw(PARAM_INDEX_EG2_release, k);
    h.setParamRaw(PARAM_INDEX_Filter__Macro_lpfcut, 134217727); h.setParamRaw(PARAM_INDEX_BBD__Delay_drywet, 0);
    run(h, 4800);
    h.midi(0, 0x90, 96, 127); run(h, 48000); h.midi(0, 0x80, 96, 0);   // a high note: one period fits in a 0.5 ms window
    const int W = 24;                      // 0.5 ms windows: peak level
    static float e[2000]; int nw = 48000 / W; float mx = 0; int imx = 0;
    for (int w = 0; w < nw; w++) { float m = 0; for (int i = 0; i < W; i++) { float a = fabsf(bl[w * W + i]); if (a > m) m = a; } e[w] = m; if (m > mx) { mx = m; imx = w; } }
    int s = 0; while (s < nw && e[s] < mx * 0.02f) s++;
    int a90 = s; while (a90 < nw && e[a90] < mx * 0.9f) a90++;
    int d40 = imx; while (d40 < nw && e[d40] > mx * 0.01f) d40++;
    printf("knob %3.0f%%: attack to 90%% %6.1f ms, decay peak to -40 dB %7.1f ms\n", p * 100, (a90 - s) * 0.5, (d40 - imx) * 0.5);
  }
}
