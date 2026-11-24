// v11_test.cpp - checks the 1.1 engine changes on the generated engine (48 kHz, 16-sample blocks)
#include "../engine/ms20_host.h"
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <math.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
static const int32_t NORMAL = (int32_t)0x80000000;
static EngineCore *fresh() { EngineCore *c = new EngineCore(); c->init(); c->root.PExch[PARAM_INDEX_Filter__Macro_clicknoise].value = 0; for (int i = 0; i < 300; i++) c->process(); return c; }
static void cable(EngineCore *c, int t, int a, int b) { c->midi(0xB0, 99, t); c->midi(0xB0, 98, a); c->midi(0xB0, 6, b); }
static std::vector<float> run(EngineCore *c, double secs) { std::vector<float> o; int nb = (int)(secs * 3000); for (int b = 0; b < nb; b++) { c->process(); for (int i = 0; i < 16; i++) o.push_back(c->audioOutL[i] / 134217728.f); } return o; }
// pitch in 40 ms windows from upward zero crossings (after a DC-removing high-pass)
static std::vector<double> pitches(const std::vector<float> &x) {
  std::vector<double> r; const int W = 1920; double hp = 0, px = 0, prev = 0;
  for (size_t w = 0; w + W <= x.size(); w += W) { int n = 0; double first = -1, last = -1;
    for (int i = 0; i < W; i++) { double v = x[w + i]; hp = 0.995 * (hp + v - px); px = v; if (prev <= 0 && hp > 0) { double t = i - hp / (hp - prev); if (first < 0) first = t; last = t; n++; } prev = hp; }
    if (n > 2) r.push_back((n - 1) * 48000.0 / (last - first)); }
  return r; }
static double spreadCents(const std::vector<double> &p) { double lo = 1e9, hi = 0; for (size_t i = 2; i < p.size(); i++) { lo = fmin(lo, p[i]); hi = fmax(hi, p[i]); } return 1200 * log2(hi / lo); }
static double mean(const std::vector<double> &p) { double s = 0; int n = 0; for (size_t i = 2; i < p.size(); i++) { s += p[i]; n++; } return n ? s / n : 0; }

int main() {
  printf("MG TRI OUT follows WAVEFORM\n");
  { EngineCore *c = fresh(); c->root.PExch[PARAM_INDEX_MG__Morphing__LFO_shape].value = 1 << 26;   // WAVEFORM half-way: square
    int sq = 0, n = 0; for (int b = 0; b < 3000; b++) { c->process(); int32_t t = c->o_MG_Morphing_LFO.outlet_tri; n++; if (abs(t) > (1 << 27) * 0.95) sq++; }
    CHECK(sq > n * 0.8, "WAVEFORM at square: MG TRI OUT is a square");
    c->root.PExch[PARAM_INDEX_MG__Morphing__LFO_shape].value = 0; for (int i = 0; i < 300; i++) c->process();
    bool same = true; for (int b = 0; b < 1000; b++) { c->process(); if (c->o_MG_Morphing_LFO.outlet_tri != c->o_MG_Morphing_LFO.outlet_shaped) same = false; }
    CHECK(same, "WAVEFORM at triangle, TOTAL free: MG TRI OUT = the MG");
    delete c; }

  printf("Mono + arp: VCO 1 phaser saw\n");
  { double d[2];
    for (int k = 0; k < 2; k++) { EngineCore *c = fresh();
      cable(c, 4, 31, 25);                                       // mono
      c->root.PExch[PARAM_INDEX_MS20__Voices_arpmode].value = 30 << 21;   // arp
      c->root.PExch[PARAM_INDEX_MS20__Voices_vco1wave].value = 1;         // saw (phaser saw)
      c->root.PExch[PARAM_INDEX_MS20__Voices_pw].value = k ? (int32_t)(0.6 * 134217728) : 0;
      c->midi(0x90, 48, 100); c->midi(0x90, 52, 100); auto o = run(c, 1.0);
      double e = 0; for (float v : o) e += v * v; d[k] = e;
      if (k) CHECK(c->o_MS20_Voices.ph_fb2 != 0.f, "the arp's VCO 1 runs through the phaser");
      delete c; }
    CHECK(fabs(d[1] - d[0]) / d[0] > 0.01, "PW (phaser amount) changes the mono-arp sound"); }

  printf("Wheel: vibrato off while WHEEL OUT is patched\n");
  { EngineCore *c = fresh(); c->midi(0xB0, 1, 127); c->midi(0x90, 57, 100);
    auto spread = [&](double secs) { double lo = 1e9, hi = 0; int nb = (int)(secs * 3000);   // VCO 1's own pitch (phase step) on the playing voice
      for (int b = 0; b < nb; b++) { c->process(); for (int v = 0; v < 8; v++) if (c->o_MS20_Voices.vgn[v] > 0.5f) { double f = c->o_MS20_Voices.inc1[v]; if (b > 300) { lo = fmin(lo, f); hi = fmax(hi, f); } } }
      return hi > 0 ? 1200 * log2(hi / lo) : -1; };
    double a = spread(1.0); cable(c, 4, 27, 27); double b = spread(1.0);
    printf("    VCO 1 pitch spread: %.1f cents with the wheel up, %.1f cents with WHEEL OUT patched\n", a, b);
    CHECK(c->o_PatchBay.wplug == 1, "the patch bay sees a plug in WHEEL OUT");
    CHECK(a > 40 && b < 15, "vibrato with the wheel up, none while WHEEL OUT is patched");
    cable(c, 5, 27, 27); double d = spread(1.0);
    CHECK(c->o_PatchBay.wplug == 0 && d > 40, "plug pulled: vibrato back");
    delete c; }

  printf("VCO1+2 CV IN\n");
  { EngineCore *c = fresh(); c->root.PExch[PARAM_INDEX_MS20__Voices_vco1wave].value = 1; c->midi(0xB0, 74, 127);
    c->midi(0x90, 45, 100); auto a = pitches(run(c, 0.6));
    cable(c, 4, 10, 25); run(c, 0.01);                          // EG1 OUT -> VCO1+2 CV IN
    CHECK(c->o_PatchBay.sel12 == 3 && c->o_PatchBay.monof == 0, "EG1 OUT in VCO1+2 CV IN: routed, not mono");
    CHECK(c->o_PatchBay.outlet_vco12cv != NORMAL, "the patch bay sends EG1 to VCO1+2 CV");
    auto b = pitches(run(c, 0.6));
    printf("    pitch %.0f Hz -> %.0f Hz with EG1 in VCO1+2 CV IN\n", mean(a), mean(b));
    CHECK(mean(b) > mean(a) * 1.5, "both VCOs go up with the CV");
    cable(c, 5, 10, 25); cable(c, 5, 25, 25); run(c, 0.01);
    CHECK(c->o_PatchBay.sel12 == 0 && c->o_PatchBay.outlet_vco12cv == NORMAL, "cable pulled: back to normal");
    cable(c, 4, 31, 25); run(c, 0.01);
    CHECK(c->o_PatchBay.monof == 1 && c->o_PatchBay.outlet_vco12cv == NORMAL, "KBD CV OUT in VCO1+2 CV IN: mono, no pitch offset");
    delete c; }

  printf("Volume at 0 = off\n");
  { EngineCore *c = fresh(); int p0 = c->o_PatchBay.pon;
    c->midi(0xB0, 7, 0); CHECK(c->o_PatchBay.pwr == 0, "CC 7 = 0: power off");
    c->midi(0xB0, 7, 90); CHECK(c->o_PatchBay.pwr == 1 && c->o_PatchBay.pon == p0 + 1, "CC 7 up again: power on, once");
    cable(c, 6, 0, 127); CHECK(c->o_PatchBay.pon == p0 + 1, "the iC's own power-on after it: not counted twice");
    c->midi(0xB0, 7, 40); CHECK(c->o_PatchBay.pon == p0 + 1, "volume moves while on: nothing happens");
    delete c; }

  printf("INITIAL GAIN: drone voice\n");
  { auto level = [](EngineCore *c, double secs) { double e = 0; int n = 0; for (int b = 0; b < (int)(secs * 3000); b++) { c->process(); for (int i = 0; i < 16; i++) { double v = c->audioOutL[i] / 134217728.0; e += v * v; n++; } } return sqrt(e / n); };
    EngineCore *c = fresh(); c->midi(0xB0, 74, 100);
    c->midi(0x90, 60, 100); level(c, 0.5); c->midi(0x80, 60, 0); level(c, 3.0);
    double silent = level(c, 0.5);
    cable(c, 4, 27, 21); c->midi(0xB0, 1, 127);                // WHEEL OUT -> INITIAL GAIN, wheel up
    double drone = level(c, 1.0);
    c->midi(0x90, 67, 100); level(c, 0.3); c->midi(0x80, 67, 0); double after = level(c, 3.0);
    c->midi(0xB0, 1, 0); level(c, 3.0); double off = level(c, 0.5);
    printf("    level: no cable %.5f, drone %.4f, after a new note %.4f, wheel down %.5f\n", silent, drone, after, off);
    CHECK(silent < 0.002, "no cable: silent after the release (only the delay tail)");
    CHECK(drone > 0.01, "INITIAL GAIN patched, wheel up: the last note drones");
    CHECK(after > 0.01, "a new note takes over the drone");
    CHECK(off < 0.002, "source at zero: the drone closes");
    CHECK(c->o_PatchBay.selig == 9, "INITIAL GAIN's source is the wheel");
    delete c; }

  printf("Modulation VCA: VCA IN x CONTROL IN -> VCA OUT\n");
  { EngineCore *c = fresh(); c->root.PExch[PARAM_INDEX_EG1_amount].value = 0x07FFFFFF;   // EG1/EXT knob up: scales the FREQ jack
    c->root.PExch[PARAM_INDEX_MG__Morphing__LFO_rate].value = 40 << 20;
    c->midi(0x90, 57, 100);
    auto spread = [&](double secs) { double lo = 1e9, hi = 0; int nb = (int)(secs * 3000);
      for (int b = 0; b < nb; b++) { c->process(); for (int v = 0; v < 8; v++) if (c->o_MS20_Voices.vgn[v] > 0.5f) { double f = c->o_MS20_Voices.inc1[v]; if (b > 300) { lo = fmin(lo, f); hi = fmax(hi, f); } } }
      return hi > 0 ? 1200 * log2(hi / lo) : -1; };
    cable(c, 4, 1, 12); cable(c, 4, 16, 5); run(c, 0.01);   // MG TRI -> VCA IN, VCA OUT -> FREQ
    CHECK(c->o_PatchBay.selvin == 1 && c->root.PExch[PARAM_INDEX_PatchBay_freq].value == 5, "VCA IN has the MG, FREQ has VCA OUT");
    int32_t mx = 0; for (int b = 0; b < 3000; b++) { c->process(); mx = std::max(mx, abs(c->o_PatchBay.src[5])); }
    double a = spread(1.5);
    CHECK(mx == 0, "nothing in CONTROL IN: the VCA is closed");
    cable(c, 4, 27, 14); c->midi(0xB0, 1, 127); run(c, 0.05);   // WHEEL OUT -> CONTROL IN, wheel up
    for (int b = 0; b < 3000; b++) { c->process(); mx = std::max(mx, abs(c->o_PatchBay.src[5])); }
    double b2 = spread(1.5);
    printf("    VCO 1 pitch spread: %.1f cents closed, %.1f cents with the wheel up in CONTROL IN\n", a, b2);
    CHECK(mx > (1 << 25), "the wheel opens the VCA: the MG comes out of VCA OUT");
    CHECK(a < 5 && b2 > 50, "the MG reaches the pitch only through the open VCA");
    CHECK(c->o_PatchBay.outlet_vca == NORMAL, "VCA OUT patched: CONTROL IN leaves the main volume alone");
    cable(c, 5, 16, 5); cable(c, 5, 5, 5); cable(c, 5, 16, 16); run(c, 0.01);
    CHECK(c->o_PatchBay.outlet_vca != NORMAL, "VCA OUT unplugged: CONTROL IN is the main volume again (tremolo)");
    cable(c, 5, 1, 12); cable(c, 5, 12, 12); run(c, 0.01);
    CHECK(c->o_PatchBay.selvin == 0 && c->o_PatchBay.src[5] == 0, "VCA IN unplugged: nothing in the VCA");
    delete c; }

  printf("Retrigger in mono\n");
  { int edges[2];
    for (int mt = 0; mt < 2; mt++) { EngineCore *c = fresh();
      cable(c, 4, 31, 25); run(c, 0.01);                                   // mono
      c->root.PExch[PARAM_INDEX_MS20__Voices_multitrig].value = mt;
      int e = 0, pg = 0; auto tick = [&](int n) { for (int i = 0; i < n; i++) { c->process(); int g = c->o_MS20_Voices.outlet_polygate ? 1 : 0; if (g && !pg) e++; pg = g; } };
      c->midi(0x90, 57, 100); tick(1500); c->midi(0x90, 60, 100); tick(1500); c->midi(0x90, 62, 100); tick(1500);   // legato: three keys held
      edges[mt] = e; delete c; }
    printf("    keyboard gate starts: %d with retrigger off, %d with retrigger on (three legato keys)\n", edges[0], edges[1]);
    CHECK(edges[0] == 1, "retrigger off: legato keys only change the pitch (one attack)");
    CHECK(edges[1] == 3, "retrigger on: every new key attacks again (EG 1 and EG 2 see the gate start)"); }

  printf("ESP knobs: pickup after a layer change\n");
  { EngineCore *c = fresh();
    c->root.PExch[PARAM_INDEX_MS20__Voices_arpmode].value = 30 << 21; run(c, 0.01);   // arp on: CC 88 = arp rate
    const int T = PARAM_INDEX_BBD__Delay_time;
    c->root.PExch[T].value = 63 << 20;                                       // delay time where the knob is not
    c->midi(0xB0, 88, 100); run(c, 0.01);
    CHECK(c->root.PExch[PARAM_INDEX_MS20__Voices_arprate].value == (100 << 20), "arp layer: the knob sets the arp rate");
    c->midi(0xD0, 127, 0); run(c, 0.01);                                     // switch held: CC 88 = delay time
    c->midi(0xB0, 88, 101); run(c, 0.01);
    CHECK(c->root.PExch[T].value == (63 << 20), "delay layer: the knob does not jump the delay time to the arp rate's position");
    c->midi(0xB0, 88, 70); run(c, 0.01);
    CHECK(c->root.PExch[T].value == (63 << 20), "moving towards it: still not");
    c->midi(0xB0, 88, 60); run(c, 0.01);
    CHECK(c->root.PExch[T].value == (60 << 20), "passed the delay time's value: the knob has it");
    c->midi(0xB0, 88, 40); run(c, 0.01);
    CHECK(c->root.PExch[T].value == (40 << 20), "and follows from then on");
    c->midi(0xD0, 0, 0); run(c, 0.01);                                       // released: CC 88 = arp rate again (was 100)
    c->midi(0xB0, 88, 41); run(c, 0.01);
    CHECK(c->root.PExch[PARAM_INDEX_MS20__Voices_arprate].value == (100 << 20), "back on the arp layer: the arp rate waits for the knob too");
    cable(c, 6, 0, 0); run(c, 0.01); cable(c, 6, 0, 127); run(c, 0.01);      // volume off and on: the panel is read
    c->midi(0xB0, 88, 42); run(c, 0.01);
    CHECK(c->root.PExch[PARAM_INDEX_MS20__Voices_arprate].value == (42 << 20), "power on: every knob is where it is (no pickup)");
    delete c; }

  printf("ESP layer latch (CC 105) and the function switch (CC 104)\n");
  { EngineCore *c = fresh();
    c->root.PExch[PARAM_INDEX_MS20__Voices_arpmode].value = 30 << 21; run(c, 0.01);
    CHECK(c->o_ESP_Layer.outlet_layerb == 0, "arp on: the ESP knobs set the arp");
    c->midi(0xB0, 105, 127); run(c, 0.01);
    CHECK(c->o_ESP_Layer.outlet_layerb == 1, "latch on: the delay layer, no switch held");
    c->midi(0xB0, 105, 0); run(c, 0.01);
    CHECK(c->o_ESP_Layer.outlet_layerb == 0, "latch off: the arp again");
    c->midi(0xB0, 104, 127); run(c, 0.01);
    CHECK(c->o_Patch_Memory.armed && c->o_ESP_Layer.outlet_layerb == 1, "function switch held: memory keys armed, delay layer");
    c->midi(0xB0, 104, 0); run(c, 0.01);
    c->midi(0xD0, 127, 0); run(c, 0.01);
    CHECK(!c->o_Patch_Memory.armed && c->o_ESP_Layer.outlet_layerb == 0, "with a function switch, the iC's switch no longer arms memory or holds the layer");
    CHECK(c->o_MS20_Voices.outlet_button == 1, "... it is the patch bay SWITCH");
    c->midi(0xD0, 0, 0); run(c, 0.01);
    delete c; }

  printf("Tap tempo button (CC 106)\n");
  { EngineCore *c = fresh(); const int T = PARAM_INDEX_BBD__Delay_time; const int32_t before = c->root.PExch[T].value;
    for (int k = 0; k < 4; k++) { c->midi(0xB0, 106, 127); run(c, 0.5); }      // four taps, 0.5 s apart
    const double ms = (1400.0 + (0.45 * (c->root.PExch[T].value / 134217728.0) + 0.55 * pow(c->root.PExch[T].value / 134217728.0, 2)) * 126000.0) / 48.0;
    printf("    delay time after four taps 0.5 s apart: %.0f ms\n", ms);
    CHECK(c->root.PExch[T].value != before && fabs(ms - 500.0) < 15.0, "the taps set the delay time (no tap cable needed)");
    delete c; }

  printf("TRIG LED: the delay time\n");
  { int fl[2], dt = 0;
    for (int k = 0; k < 2; k++) { EngineCore *c = fresh();
      c->root.PExch[PARAM_INDEX_BBD__Delay_drywet].value = k ? (40 << 21) : 0;
      c->root.PExch[PARAM_INDEX_BBD__Delay_time].value = 10 << 21;
      run(c, 0.2); dt = c->o_BBD_Delay.outlet_dtick;
      int f = 0, pw = 0; for (int i = 0; i < 6000; i++) { c->process(); int w = c->o_MS20iC_LEDs.want[3]; if (w && !pw) f++; pw = w; }
      fl[k] = f; delete c; }
    printf("    delay %d ticks (%.0f ms): %d flashes in 2 s with DELAY AMOUNT up, %d with it down\n", dt, dt / 3.0, fl[1], fl[0]);
    CHECK(fl[0] == 0, "DELAY AMOUNT down: the TRIG LED stays dark");
    CHECK(dt > 0 && fl[1] >= 6000 / dt - 1 && fl[1] <= 6000 / dt + 1, "DELAY AMOUNT up: one flash per repeat"); }

  printf(fails ? "V11 CHECKS FAILED (%d)\n" : "ALL V11 CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
