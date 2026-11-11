// mk_test.cpp - the microKONTROL controller layer (engine/mk_control.h) against the real engine.
// Build: clang++ -std=c++17 -O2 -w -o mk_test mk_test.cpp && ./mk_test
#define MS20HOST_TESTING
#include "../engine/ms20_host.h"
#include "../engine/mk_control.h"
#include <stdio.h>
#include <vector>
#include <string>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 0x12345678u; }
using namespace ms20;

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

struct Sink : MkSink {
  MS20Host *e; std::vector<std::vector<uint8_t>> out;
  int32_t get(int i) override { return e->getParamRaw(i); }
  void set(int i, int32_t raw) override { e->setParamNow(i, raw); }
  void midi(uint8_t s, uint8_t a, uint8_t b) override { e->midi(0, s, a, b); }
  int memSlot() override { return e->memSlot(); }
  void sysex(const uint8_t *b, int n) override { out.push_back(std::vector<uint8_t>(b, b + n)); }
  std::string lcd(int n) {   // the last text written to display n
    for (int k = (int)out.size() - 1; k >= 0; k--) { auto &m = out[k]; if (m.size() == 17 && m[5] == 0x22 && (m[7] & 15) == n) return std::string(m.begin() + 8, m.begin() + 16); }
    return "";
  }
  int led(int n) { for (int k = (int)out.size() - 1; k >= 0; k--) { auto &m = out[k]; if (m.size() == 9 && m[5] == 0x01 && m[6] == n) return (m[7] >> 5) & 3; } return -1; }
};

static MS20Host E; static Sink K; static uint32_t ms = 0;
static void run(double secs) { float L[256], R[256]; for (int i = 0; i < (int)(secs * 48000 / 256); i++) { E.process(L, R, 256); ms += 5; } }

int main() {
  E.reset(48000); K.e = &E;
  static MicroKontrol<NPARAMS>::Info info[NPARAMS];
  for (int i = 0; i < NPARAMS; i++) info[i] = {kParamInfo[i].object, kParamInfo[i].name, kParamInfo[i].type, kParamInfo[i].hmax};
  MicroKontrol<NPARAMS> mk(info);
  check(mk.complete(), "every encoder and slider finds its setting in the engine");

  mk.start(K);
  check(K.out.size() > 16 && K.out[0] == std::vector<uint8_t>({0xF0, 0x42, 0x40, 0x6E, 0x00, 0x00, 0x00, 0x01, 0xF7}) &&
        K.out[15] == std::vector<uint8_t>({0xF0, 0x42, 0x4F, 0x6E, 0x00, 0x00, 0x00, 0x01, 0xF7}), "start: native mode on, asked on all 16 global channels");
  check(!mk.nativeOn(), "not yet confirmed by the controller");
  // the controller answers on its global channel (here 3): from now on everything goes out on it
  mk.setGlobal(2); mk.native(0x40, 0x00, 0x03, K, ms);
  check(mk.nativeOn() && K.out.back()[2] == 0x42, "the reply sets the channel: messages go out as F0 42 42 ...");
  mk.setGlobal(0);
  check(K.lcd(8) == "VCO 1/5 ", "main display: page name");
  check(K.lcd(2) == "PW      ", "small displays: the encoders' names");
  { auto &m = K.out[16]; check(m.size() == 17 && m[0] == 0xF0 && m[5] == 0x22 && m[6] == 0x09 && m[16] == 0xF7, "display messages are F0 42 40 6E 00 22 09 bn c1..c8 F7"); }

  // encoder 3 (PW): +10 detents = +5 knob units
  const int pw = mk.encoderTarget(0, 2);
  const int32_t pw0 = E.getParamRaw(pw);
  mk.native(0x43, 2, 10, K, ms); run(0.05);
  check(E.getParamRaw(pw) == pw0 + 5 * (1 << 21), "encoder: +10 detents moves PW by 5 units");
  check(K.lcd(2).find(".") != std::string::npos, "the turned encoder shows its value");
  mk.native(0x43, 2, 0x7F, K, ms); run(0.05);   // -1
  check(E.getParamRaw(pw) == pw0 + 9 * (1 << 20), "encoder: -1 detent (0x7F) moves back half a unit");
  run(1.2); mk.tick(K, ms);
  check(K.lcd(2) == "PW      ", "after a second the display shows the name again");

  // a switch-type setting: VCO 1 waveform, one step per 3 detents
  const int w = mk.encoderTarget(0, 0); const int32_t w0 = E.getParamRaw(w);
  mk.native(0x43, 0, 1, K, ms); mk.native(0x43, 0, 1, K, ms);
  check(E.getParamRaw(w) == w0, "waveform: two detents are not yet a step");
  mk.native(0x43, 0, 1, K, ms);
  check(E.getParamRaw(w) == (w0 < 3 ? w0 + 1 : w0), "waveform: the third detent steps");

  // pages
  mk.native(0x48, 1, 127, K, ms);
  check(mk.page() == 1 && K.lcd(8) == "EG  2/5 " && K.lcd(0) == "EG1 DLY ", "> : next page (EG), names follow");
  mk.native(0x48, 0, 127, K, ms); mk.native(0x48, 0, 127, K, ms);
  check(mk.page() == 4 && K.lcd(8) == "OUT 5/5 ", "< wraps around to the last page");
  mk.native(0x48, 1, 127, K, ms);
  check(mk.page() == 0, "> wraps to the first page");

  // slider 5 (LPF cutoff) with soft takeover: the cutoff sits in the middle
  const int lc = mk.sliderTarget(4);
  E.setParamNow(lc, 0); run(0.05);
  mk.native(0x44, 4, 0, K, ms); run(0.05);
  check(E.getParamRaw(lc) == 0, "slider at the bottom: not yet taken over (cutoff stays)");
  check(K.lcd(4)[0] == '^', "the display shows where the cutoff is (^)");
  mk.native(0x44, 4, 40, K, ms); run(0.05);
  check(E.getParamRaw(lc) == 0, "still below the current value: nothing changes");
  mk.native(0x44, 4, 70, K, ms); run(0.05);
  check(E.getParamRaw(lc) > 0, "passing the current value: the slider takes over");
  mk.native(0x44, 4, 127, K, ms); run(0.05);
  check(E.getParamRaw(lc) == 64 * (1 << 21), "slider at the top: cutoff fully open");

  // memories on the pads: hold pad 3 for 2.2 s = save; change the sound; tap pad 3 = recall
  const int32_t saved = E.getParamRaw(pw);
  mk.native(0x45, 0x40 | 2, 100, K, ms); run(2.3); mk.native(0x45, 2, 64, K, ms); run(0.1);
  mk.native(0x43, 2, 20, K, ms); run(0.05);
  check(E.getParamRaw(pw) != saved, "the sound changed after saving");
  mk.native(0x45, 0x40 | 2, 100, K, ms); run(0.1); mk.native(0x45, 2, 64, K, ms); run(0.3);
  check(E.getParamRaw(pw) == saved, "tap pad 3: memory 3 recalled (PW back)");
  { E.setParamNow(lc, 10 << 21); run(0.05);   // something else (a memory, the DAW) moved the cutoff
    mk.native(0x44, 4, 126, K, ms); run(0.05);
    check(E.getParamRaw(lc) == (10 << 21), "when the cutoff was moved elsewhere the slider waits again (no jump)"); }
  mk.tick(K, ms);
  check(E.memSlot() == 2 && K.led(2) == 1 && K.led(3) == 0, "the active memory's pad is lit");
  // ENTER: the panel
  mk.native(0x48, 2, 127, K, ms); run(0.3); mk.tick(K, ms);
  check(E.memSlot() == -1 && K.led(2) == 0, "ENTER: back to the panel, pad light off");
  // SCENE held: pads are memories 17-32
  mk.native(0x48, 5, 127, K, ms);
  mk.native(0x45, 0x40 | 0, 100, K, ms); run(2.3); mk.native(0x45, 0, 64, K, ms); run(0.1);
  mk.native(0x48, 5, 0, K, ms); mk.tick(K, ms);
  check(E.memSlot() == 16, "SCENE + pad 1 (held): saved to memory 17");

  // joystick and pedal reach the engine without trouble
  mk.native(0x4B, 127, 127, K, ms); mk.native(0x4B, 64, 64, K, ms); mk.native(0x47, 0, 127, K, ms); mk.native(0x47, 0, 0, K, ms); run(0.1);
  check(true, "joystick and pedal accepted");
  // native-mode reply redraws
  const size_t before = K.out.size(); mk.native(0x40, 0, 0x03, K, ms);
  check(K.out.size() >= before + 9, "the controller confirms native mode: everything is drawn");
  mk.stop(K);
  check(K.out.back() == std::vector<uint8_t>({0xF0, 0x42, 0x40, 0x6E, 0x00, 0x00, 0x00, 0x00, 0xF7}), "stop: native mode off");

  printf("\n%s\n", fails ? "SOME MICROKONTROL CHECKS FAILED" : "ALL MICROKONTROL CHECKS PASSED");
  return fails ? 1 : 0;
}
