// mem_test.cpp - patch memory in the plugin: recall, save to file, reload in a new instance, survive a reset.
#include "../engine/ms20_host.h"
#include <stdio.h>
#include <sys/stat.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
static float bl[512], br[512];
static void run(MS20Host &h, double sec) { int n = (int)(sec * 48000); while (n > 0) { int k = n > 512 ? 512 : n; h.process(bl, br, k); n -= k; } }
static void ev(MS20Host &h, uint8_t s, uint8_t a, uint8_t b) { h.midi(0, s, a, b); run(h, 0.005); }
int main() {
  const char *dir = "./memtest/"; mkdir(dir, 0755); remove("./memtest/MS20MEM.BIN");
  const int LPF = PARAM_INDEX_Filter__Macro_lpfcut, PW = PARAM_INDEX_MS20__Voices_pw;
  printf("1. Recall a factory sound from the keys\n");
  { MS20Host h; h.reset(44100); bool had = h.loadMemories(dir); CHECK(!had, "no file yet: factory sounds");
    ev(h, 0xB0, 74, 100); run(h, 0.1); int32_t before = h.getParamRaw(LPF);
    ev(h, 0xD0, 127, 0); ev(h, 0x90, 48 + 15, 100); ev(h, 0x80, 48 + 15, 0); ev(h, 0xD0, 0, 0); run(h, 0.1);   // key 16 = Soft Keys (LPF 30)
    printf("     LPF cutoff before %.1f, after recall %.1f (Soft Keys stores 30.0)\n", before / 2097152.0, h.getParamRaw(LPF) / 2097152.0);
    CHECK(h.getParamRaw(LPF) == 30 << 21, "Soft Keys recalled (LPF cutoff = 30)");
    printf("2. Save a sound on a key, written to the file\n");
    ev(h, 0xB0, 14, 90); ev(h, 0xB0, 14, 92); ev(h, 0xB0, 14, 97); run(h, 0.1);              // move PW (scaling pickup)
    int32_t pw = h.getParamRaw(PW);
    ev(h, 0xD0, 127, 0); ev(h, 0x90, 48 + 20, 100); run(h, 2.2); ev(h, 0x80, 48 + 20, 0); ev(h, 0xD0, 0, 0);
    h.pollMemorySave();
    FILE *f = fopen("./memtest/MS20MEM.BIN", "rb"); CHECK(f != nullptr, "MS20MEM.BIN written by the main-thread poll"); if (f) fclose(f);
    printf("3. A sample-rate change keeps the memories\n");
    h.reset(48000); ev(h, 0xD0, 127, 0); ev(h, 0x90, 48, 100); ev(h, 0x80, 48, 0); ev(h, 0x90, 48 + 20, 100); ev(h, 0x80, 48 + 20, 0); ev(h, 0xD0, 0, 0); run(h, 0.1);
    CHECK(h.getParamRaw(PW) == pw, "saved sound still there after reset");
    printf("4. A new plugin instance loads the saved sound\n");
    MS20Host h2; h2.reset(44100); CHECK(h2.loadMemories(dir), "file loaded");
    ev(h2, 0xD0, 127, 0); ev(h2, 0x90, 48 + 20, 100); ev(h2, 0x80, 48 + 20, 0); ev(h2, 0xD0, 0, 0); run(h2, 0.1);
    CHECK(h2.getParamRaw(PW) == pw, "saved sound recalled in the new instance");
    printf("5. Keys are silent while the switch is held\n");
    float pk = 0; ev(h2, 0xD0, 127, 0); h2.midi(0, 0x90, 60, 100); for (int i = 0; i < 40; i++) { h2.process(bl, br, 512); for (int j = 0; j < 512; j++) pk = fmaxf(pk, fabsf(bl[j])); }
    CHECK(pk < 1e-3f, "no sound from a memory key"); ev(h2, 0x80, 60, 0); ev(h2, 0xD0, 0, 0);
  }
  printf("\n%s (%d failed)\n", fails ? "SOME MEMORY CHECKS FAILED" : "ALL PLUGIN MEMORY CHECKS PASSED", fails); return fails;
}
