// cables_test.cpp - 1.1: sounds remember their cables (and old memory files still load)
#include "../engine/ms20_host.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
static float bl[512], br[512];
static void run(MS20Host &h, double sec) { int n = (int)(sec * 48000); while (n > 0) { int k = n > 512 ? 512 : n; h.process(bl, br, k); n -= k; } }
static void ev(MS20Host &h, uint8_t s, uint8_t a, uint8_t b) { h.midi(0, s, a, b); run(h, 0.005); }
static void cable(MS20Host &h, int t, int a, int b) { ev(h, 0xB0, 99, t); ev(h, 0xB0, 98, a); ev(h, 0xB0, 6, b); }
static void tapKey(MS20Host &h, int n) { ev(h, 0xD0, 127, 0); ev(h, 0x90, 48 + n, 100); ev(h, 0x80, 48 + n, 0); ev(h, 0xD0, 0, 0); run(h, 0.02); }
static void holdKey(MS20Host &h, int n) { ev(h, 0xD0, 127, 0); ev(h, 0x90, 48 + n, 100); run(h, 2.2); ev(h, 0x80, 48 + n, 0); ev(h, 0xD0, 0, 0); run(h, 0.02); }
static int32_t P(MS20Host &h, int i) { return h.getParamRaw(i); }

int main() {
  const char *dir = "./cabtest/"; mkdir(dir, 0755); remove("./cabtest/MS20MEM.BIN");
  const int LPF = PARAM_INDEX_PatchBay_lpf, HPF = PARAM_INDEX_PatchBay_hpf, OVR = PARAM_INDEX_Patch__State_ovr;
  const int MONO = PARAM_INDEX_Patch__State_mono, WHL = PARAM_INDEX_Patch__State_wheel, CUT = PARAM_INDEX_Filter__Macro_lpfcut;
  { MS20Host h; h.reset(48000); h.loadMemories(dir); run(h, 0.05);
    printf("1. Save a sound with its cables\n");
    cable(h, 4, 1, 17);                                   // MG TRI OUT -> LPF CUTOFF
    cable(h, 4, 31, 25);                                  // KBD CV OUT -> VCO1+2 CV IN: mono
    CHECK(P(h, LPF) == 1 && P(h, MONO) == 1, "panel: MG in LPF CUTOFF, mono");
    holdKey(h, 5); h.pollMemorySave();
    printf("2. Change the real cables, recall the sound\n");
    cable(h, 5, 1, 17); cable(h, 5, 17, 17); cable(h, 5, 31, 25); cable(h, 5, 25, 25);   // both cables out
    cable(h, 4, 10, 13);                                  // EG1 OUT -> HPF CUTOFF
    CHECK(P(h, LPF) == 0 && P(h, HPF) == 3 && P(h, MONO) == 0, "panel now: EG1 in HPF CUTOFF, poly");
    tapKey(h, 5);
    CHECK(P(h, OVR) == 1, "recalled sound's cables in use");
    CHECK(P(h, LPF) == 1 && P(h, HPF) == 0 && P(h, MONO) == 1, "the sound's cables: MG in LPF, nothing in HPF, mono");
    printf("3. Touch a cable: the real panel again\n");
    cable(h, 4, 27, 27);                                  // a plug in WHEEL OUT
    CHECK(P(h, OVR) == 0, "cable touched: sound's cables released");
    CHECK(P(h, LPF) == 0 && P(h, HPF) == 3 && P(h, MONO) == 0 && P(h, WHL) == 1, "real panel: EG1 in HPF, poly, plus the new plug");
    cable(h, 5, 27, 27);
    printf("4. A factory sound (no cables stored) keeps the real cables\n");
    tapKey(h, 5); CHECK(P(h, OVR) == 1, "own-cable sound recalled again");
    tapKey(h, 15);                                        // Soft Keys: factory, no cables
    CHECK(P(h, OVR) == 0 && P(h, HPF) == 3 && P(h, LPF) == 0, "factory sound: back to the real panel's cables");
    printf("5. Panel (switch + top C) and cables only (top C held 2 s)\n");
    tapKey(h, 5); tapKey(h, 36);
    CHECK(P(h, OVR) == 0 && P(h, HPF) == 3, "panel: real cables");
    tapKey(h, 5); int32_t cut = P(h, CUT);
    holdKey(h, 36);
    CHECK(P(h, OVR) == 0 && P(h, HPF) == 3 && P(h, LPF) == 0, "cables only: real cables");
    CHECK(P(h, CUT) == cut, "cables only: the knobs stay as recalled");
  }
  printf("6. The file keeps the cables (new instance)\n");
  { MS20Host h; h.reset(48000); CHECK(h.loadMemories(dir), "file loaded"); run(h, 0.05);
    tapKey(h, 5); CHECK(P(h, OVR) == 1 && P(h, LPF) == 1 && P(h, MONO) == 1, "sound 6 comes back with its cables");
    FILE *f = fopen("./cabtest/MS20MEM.BIN", "rb"); int32_t hd[4] = {0}; if (f) { fread(hd, 4, 4, f); fseek(f, 0, SEEK_END); long sz = ftell(f); fclose(f);
      printf("     header %08X NS=%d NP=%d NC=%d, %ld bytes\n", hd[0], hd[1], hd[2], hd[3], sz);
      CHECK(hd[0] == 0x4D533231 && hd[3] == 15 && sz == 16 + 36 * (29 + 2 + 15) * 4, "'MS21' file, 15 cable values per sound (with VCA IN)"); } }
  printf("6b. A file from the 1.1 test builds (14 cable values) still loads\n");
  { mkdir("./cabtest14/", 0755); FILE *f = fopen("./cabtest14/MS20MEM.BIN", "wb");
    int32_t hd[4] = {0x4D533231, 36, 29, 14}; fwrite(hd, 4, 4, f);
    for (int n = 0; n < 36; n++) { int32_t r[29 + 2 + 14]; memset(r, 0, sizeof r); r[0] = (n == 5); r[30] = (n == 5); r[31 + 4] = 1; fwrite(r, 4, 29 + 2 + 14, f); }   // slot 6: MG TRI in LPF CUTOFF
    fclose(f);
    MS20Host h; h.reset(48000); CHECK(h.loadMemories("./cabtest14/"), "14-cable file loaded"); run(h, 0.05);
    tapKey(h, 5); CHECK(P(h, OVR) == 1 && P(h, LPF) == 1, "its sound comes back with its cables (VCA IN stays empty)"); }
  printf("7. A 1.0 memory file ('MS20', knobs only) still loads\n");
  { mkdir("./cabtest_old/", 0755); FILE *f = fopen("./cabtest_old/MS20MEM.BIN", "wb");
    int32_t hd[3] = {0x4D533230, 36, 29}; fwrite(hd, 4, 3, f);
    for (int n = 0; n < 36; n++) { int32_t r[30]; memset(r, 0, sizeof r); r[0] = (n == 7); r[23] = 77 << 20; fwrite(r, 4, 30, f); }   // slot 8: LPF cutoff knob (index 22?) set
    fclose(f);
    MS20Host h; h.reset(48000); CHECK(h.loadMemories("./cabtest_old/"), "old file loaded"); run(h, 0.05);
    cable(h, 4, 10, 13); tapKey(h, 7);
    CHECK(P(h, OVR) == 0 && P(h, HPF) == 3, "old sound: no cables stored, the real cables stay"); }
  printf(fails ? "CABLE CHECKS FAILED (%d)\n" : "ALL CABLE CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
