// panel_test.cpp - the remembered panel (engine/panel_memory.h) against the MS-20ic's own start-up dump.
// Build: clang++ -std=c++17 -O2 -w -o panel_test panel_test.cpp && ./panel_test
//
// Engine A gets the MS-20ic's real dump (from a USB capture), then a few knob moves and a patch cable.
// The panel memory sees the same messages, is saved and loaded again, and its replay goes into engine B.
// A and B must end up with the same settings, and B's replay must start the light show (volume switched on).
#define MS20HOST_TESTING
#include "../engine/ms20_host.h"
#include "../engine/panel_memory.h"
#include "ms20ic_dump.h"
#include <stdio.h>
#include <vector>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

struct Msg { uint8_t s, d1, d2; };
static void run(MS20Host &h, int blocks) { float L[256], R[256]; for (int i = 0; i < blocks; i++) h.process(L, R, 256); }
static void feed(MS20Host &h, const std::vector<Msg> &m) { for (auto &x : m) h.midi(0, x.s, x.d1, x.d2); run(h, 20); }

int main() {
  std::vector<Msg> fromController;
  for (auto &d : kDump) fromController.push_back({d[0], d[1], d[2]});
  // after the dump: two knob moves and a cable from MG triangle out to the VCO 1+2 CV in (connect, then plug-ins)
  const Msg extra[] = {{0xB0, 0x4A, 20}, {0xB0, 0x1C, 100},
                       {0xB0, 99, 4}, {0xB0, 98, 9}, {0xB0, 6, 25}, {0xB0, 99, 4}, {0xB0, 98, 9}, {0xB0, 6, 9}, {0xB0, 99, 4}, {0xB0, 98, 25}, {0xB0, 6, 25}};
  for (auto &x : extra) fromController.push_back(x);

  MS20Host a, b; a.reset(48000); b.reset(48000);
  // both start from another sound (as if a project or a memory was loaded); settings without a knob keep it
  for (int i = 0; i < NPARAMS; i++) { const int32_t v = a.getParamRaw(i) ^ 0x100000; a.setParamRaw(i, v); b.setParamRaw(i, v); }
  run(a, 5); run(b, 5);
  PanelMemory pm;
  check(!pm.known(), "a new panel memory knows nothing");
  feed(a, fromController);
  for (auto &x : fromController) pm.onMessage(x.s, x.d1, x.d2);
  check(pm.known() && pm.powerOns() == 1, "the controller's dump is recognised (volume switched on once)");

  const char *path = "/tmp/panel_test_MS20PANEL.BIN";
  check(pm.save(path), "saved");
  PanelMemory loaded;
  check(loaded.load(path), "loaded again");
  std::vector<Msg> replay;
  loaded.replay([&](uint8_t s, uint8_t d1, uint8_t d2) { replay.push_back({s, d1, d2}); });
  printf("      replay: %zu messages (the controller's dump + moves: %zu)\n", replay.size(), fromController.size());

  feed(b, replay);
  int diff = 0;
  for (int i = 0; i < NPARAMS; i++) if (a.getParamRaw(i) != b.getParamRaw(i)) { diff++; printf("      differs: param %d (%s) %d vs %d\n", i, kParamInfo[i].label, a.getParamRaw(i), b.getParamRaw(i)); }
  check(diff == 0, "the replayed panel gives the engine exactly the settings the controller gave it");

  // the replay a second time (the controller unplugged and plugged in again): same result
  feed(b, replay);
  diff = 0; for (int i = 0; i < NPARAMS; i++) if (a.getParamRaw(i) != b.getParamRaw(i)) diff++;
  check(diff == 0, "replaying again changes nothing");

  // the cable was pulled out again later: it must not come back
  PanelMemory p2; for (auto &x : fromController) p2.onMessage(x.s, x.d1, x.d2);
  const Msg pull[] = {{0xB0, 99, 5}, {0xB0, 98, 9}, {0xB0, 6, 25}, {0xB0, 99, 5}, {0xB0, 98, 9}, {0xB0, 6, 9}, {0xB0, 99, 5}, {0xB0, 98, 25}, {0xB0, 6, 25}};
  for (auto &x : pull) p2.onMessage(x.s, x.d1, x.d2);
  int connects = 0; p2.replay([&](uint8_t s, uint8_t d1, uint8_t d2) { static int t = -1; if (d1 == 99) t = d2; if (d1 == 6 && t == 4) connects++; });
  check(connects == 0, "a cable pulled out is not replayed");

  // volume switched off last: the replay leaves it off (dark panel, no light show)
  PanelMemory p3; for (auto &x : fromController) p3.onMessage(x.s, x.d1, x.d2);
  const Msg off[] = {{0xB0, 99, 6}, {0xB0, 98, 0}, {0xB0, 6, 0}};
  for (auto &x : off) p3.onMessage(x.s, x.d1, x.d2);
  std::vector<Msg> r3; p3.replay([&](uint8_t s, uint8_t d1, uint8_t d2) { r3.push_back({s, d1, d2}); });
  check(r3.size() >= 3 && r3[r3.size() - 3].d2 == 6 && r3.back().d2 == 0, "volume left off: the replay ends with it off");
  check(replay.size() >= 3 && replay[replay.size() - 3].d2 == 6 && replay.back().d2 == 127 && replay[0].d2 == 6 && replay[2].d2 == 0,
        "volume on: the replay switches it off first and on at the end (the light show)");

  // a damaged file is refused
  FILE *f = fopen(path, "r+b"); fputc('X', f); fclose(f);
  PanelMemory bad; check(!bad.load(path) && !bad.known(), "a damaged file is ignored");
  remove(path);

  printf("\n%s\n", fails ? "SOME PANEL CHECKS FAILED" : "ALL PANEL CHECKS PASSED");
  return fails ? 1 : 0;
}
