// render_demo.cpp - plays a short demo through the generated MS-20 engine and writes demo.wav (48 kHz stereo).
#include "../engine/engine_gen.h"
#include <vector>
#include <algorithm>
#include <stdio.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
static int nled = 0;
static void onSysex(const uint8_t *b, int len, void *) { if (len == 9 && b[3] == 0x6E) nled++; }
struct Ev { double t; uint8_t s, d1, d2; };
int main() {
    static ms20::EngineCore e; e.init();
    ms20::g_sysexSender = onSysex;
    std::vector<Ev> ev;
    auto on = [&](double t, int n, int v) { ev.push_back({t, 0x90, (uint8_t)n, (uint8_t)v}); };
    auto off = [&](double t, int n) { ev.push_back({t, 0x80, (uint8_t)n, 0}); };
    auto cc = [&](double t, int c, int v) { ev.push_back({t, 0xB0, (uint8_t)c, (uint8_t)v}); };
    auto cable = [&](double t, int ty, int a, int b) { cc(t, 99, ty); cc(t, 98, a); cc(t, 6, b); };
    // 1. poly chords (0-6 s)
    int ch1[] = {48, 55, 60, 64}, ch2[] = {45, 52, 57, 60}, ch3[] = {41, 48, 53, 57};
    for (int n : ch1) on(0.2, n, 90); for (int n : ch1) off(1.9, n);
    for (int n : ch2) on(2.0, n, 90); for (int n : ch2) off(3.7, n);
    for (int n : ch3) on(3.8, n, 90); cc(4.2, 1, 90); for (int n : ch3) off(5.8, n); cc(5.8, 1, 0);
    // 2. mono with portamento (6.5-11 s): patch KBD CV -> VCO1+2 CV IN, portamento up
    cable(6.3, 4, 31, 31); cable(6.3, 4, 31, 25); cable(6.3, 4, 25, 25); cc(6.3, 5, 40);
    int mel[] = {36, 43, 48, 46, 43, 41, 43, 36};
    for (int i = 0; i < 8; i++) { on(6.5 + i * 0.5, mel[i], 100); off(6.5 + i * 0.5 + 0.55, mel[i]); }
    cable(10.8, 5, 31, 25); cable(10.8, 5, 25, 25); cc(10.8, 5, 0);
    // 3. arp latch (11.5-17 s): CC11 to latch, a chord, then release
    cc(11.3, 11, 110); for (int n : ch2) on(11.5, n, 100); for (int n : ch2) off(12.5, n);
    cc(16.5, 11, 0);
    // 4. runaway delay (17-21 s): a stab, feedback (CC89 in arp-off mode) up to full
    cc(17.0, 91, 85); cc(17.0, 88, 40); on(17.2, 60, 110); off(17.5, 60); cc(17.6, 89, 127); cc(19.5, 88, 25); cc(20.2, 88, 60); cc(21.5, 89, 50); cc(23.5, 91, 26);
    double dur = 25.0; int nblk = (int)(dur * 48000 / 16);
    std::vector<float> out; out.reserve(nblk * 32);
    size_t k = 0; std::sort(ev.begin(), ev.end(), [](const Ev &a, const Ev &b) { return a.t < b.t; });
    float pk = 0;
    for (int b = 0; b < nblk; b++) {
        double t = b * 16 / 48000.0;
        while (k < ev.size() && ev[k].t <= t) { e.midi(ev[k].s, ev[k].d1, ev[k].d2); k++; }
        e.process();
        for (int i = 0; i < 16; i++) { float l = e.audioOutL[i] / 134217728.f, r = e.audioOutR[i] / 134217728.f; out.push_back(l); out.push_back(r); pk = fmaxf(pk, fmaxf(fabsf(l), fabsf(r))); }
    }
    FILE *f = fopen("demo.wav", "wb"); uint32_t n = out.size() * 4, sr = 48000;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + n); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(3); w16(2); w32(sr); w32(sr * 8); w16(8); w16(32);
    fwrite("data", 1, 4, f); w32(n); fwrite(out.data(), 4, out.size(), f); fclose(f);
    printf("rendered %.0f s, peak %.2f (%.1f dBFS), LED messages sent %d\n", dur, pk, 20 * log10(pk), nled);
}
