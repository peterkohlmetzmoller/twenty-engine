// cc_test.cpp - CC learn (engine/cc_map.h): learning, the MS-20ic's own controllers, one controller per parameter,
// forgetting, and the map file (by parameter name).
#include "../engine/cc_map.h"
#include <cstdio>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
int main()
{
  const int LPF = PARAM_INDEX_Filter__Macro_lpfcut, PITCH = PARAM_INDEX_MS20__Voices_vco2semi;
  printf("Learning\n");
  { CcMap m;
    CHECK(m.onCc(21, false) == -1, "nothing learned: a controller does nothing extra");
    m.learning.store(LPF); CHECK(m.onCc(21, false) == LPF && m.ccOf(LPF) == 21 && m.learning.load() == -1, "LPF waits, CC 21 moves: learned, and it moves LPF right away");
    CHECK(m.learnedCc.exchange(-1) == 21 && m.learnedCc.load() == -1, "the window hears of it once");
    CHECK(m.onCc(21, false) == LPF && m.onCc(22, false) == -1, "CC 21 moves LPF, CC 22 nothing");
    m.learning.store(LPF); m.onCc(40, false);
    CHECK(m.ccOf(LPF) == 40 && m.onCc(21, false) == -1, "learned again: one controller per parameter (CC 21 freed)");
    m.learning.store(PITCH); m.onCc(40, false);
    CHECK(m.ccOf(PITCH) == 40 && m.ccOf(LPF) == -1, "CC 40 learned for PITCH: LPF lets it go");
    m.forget(PITCH); CHECK(m.ccOf(PITCH) == -1 && m.onCc(40, false) == -1, "forgotten");
    m.learning.store(LPF); m.onCc(123, false);
    CHECK(m.ccOf(LPF) == -1 && m.learning.load() == LPF, "all notes off (CC 123) is never learned; still waiting"); }
  printf("MS-20ic mode: the iC's own controllers stay its own\n");
  { CcMap m;
    m.learning.store(PITCH); m.onCc(74, true);
    CHECK(m.ccOf(PITCH) == -1 && m.learning.load() == PITCH, "CC 74 (the iC's LPF knob) is not learned in MS-20ic mode");
    m.onCc(16, true);
    CHECK(m.ccOf(PITCH) == 16, "CC 16 (not the iC's) is");
    m.learning.store(LPF); m.onCc(74, false);
    CHECK(m.onCc(74, false) == LPF && m.onCc(74, true) == -1, "learned in standard mode: CC 74 acts there, never in MS-20ic mode");
    int n = 0; for (int c = 0; c < 128; c++) n += IcOwnCc(c); printf("    the iC's controllers: %d\n", n);
    CHECK(IcOwnCc(88) && IcOwnCc(99) && IcOwnCc(7) && IcOwnCc(1) && !IcOwnCc(16) && !IcOwnCc(64), "ESP knobs, cables, volume and wheel are the iC's; CC 16 and the pedal are not"); }
  printf("The map file\n");
  { CcMap a; a.learning.store(LPF); a.onCc(21, false); a.learning.store(PITCH); a.onCc(16, false);
    CHECK(a.save("/tmp/te_ccmap.txt"), "saved");
    CcMap b; CHECK(b.load("/tmp/te_ccmap.txt") && b.ccOf(LPF) == 21 && b.ccOf(PITCH) == 16, "read back by parameter name");
    FILE* f = fopen("/tmp/te_ccmap.txt", "a"); fputs("30 Not_An_Object x\n200 Filter_Macro lpfcut\n", f); fclose(f);
    CcMap c; c.load("/tmp/te_ccmap.txt");
    CHECK(c.ccOf(LPF) == 21 && c.param[30].load() == -1, "lines it does not know are skipped"); }
  printf(fails ? "CC CHECKS FAILED (%d)\n" : "ALL CC CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
