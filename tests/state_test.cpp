// state_test.cpp - every saved-state layout we have shipped or tested is read with the right parameter count, and
// the window's blocks after the takes are found (engine/state_format.h; the position logic is UnserializeState's):
// 1.0.x (58 parameters, AU and VST3), 1.1 test builds (58/63/64, with the window's blocks), 1.1 with its header,
// takes stored, and states of another parameter count (a later version reading 1.1, 1.1 reading a later version).
// The takes block is as Recall::serialize writes it: 'TER3', the parameter count, the takes (12 bytes when none).
#include "../engine/state_format.h"
#include <cstdio>
#include <vector>
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
static void put32(std::vector<uint8_t>& v, int32_t x) { const uint8_t* b = (const uint8_t*)&x; v.insert(v.end(), b, b + 4); }
static void putd(std::vector<uint8_t>& v, double x) { const uint8_t* b = (const uint8_t*)&x; v.insert(v.end(), b, b + 8); }
static std::vector<uint8_t> state(bool header, int np, int takes, bool window, bool vst3)
{
  std::vector<uint8_t> v;
  if (header) { put32(v, kStateMagicTEP2); put32(v, np); }
  for (int i = 0; i < np; i++) putd(v, i == 0 ? 2.0 : 13.5 + i);          // vco1wave = 2, then knob values
  std::vector<uint8_t> t; put32(t, kTakesMagicTER3); put32(t, np); put32(t, takes);
  for (int k = 0; k < takes; k++) { putd(t, 0.0); putd(t, 16.0); put32(t, 0); put32(t, 0); }   // a take: start, end, no events
  put32(v, (int32_t)t.size()); v.insert(v.end(), t.begin(), t.end());
  if (window) { put32(v, 0x54455531); put32(v, 1); put32(v, 0); for (int i = 0; i < 35; i++) v.push_back(0); put32(v, 0x54455532); put32(v, 1); }
  if (vst3) put32(v, 0);                                                    // bypass
  return v;
}
// what UnserializeState does with the positions: the parameters, the takes block, then 'TEU1' (47 bytes), 'TEU2' (8)
struct Read { int np, end; bool teu1, teu2; };
static Read readState(const std::vector<uint8_t>& v, int numParams)
{
  const uint8_t* d = v.data(); const int size = (int)v.size(); int pos = 0;
  Read r; r.np = StateParamCount(d, size, 0, numParams, &pos); r.teu1 = r.teu2 = false;
  pos += 8 * r.np; if (pos > size) pos = -1;
  const int e = StateTakesEnd(d, size, pos); if (e >= 0) pos = e;
  if (pos >= 0 && pos + 47 <= size && StateI32(d, pos) == 0x54455531) { pos += 47; r.teu1 = true; }
  if (pos >= 0 && pos + 8 <= size && StateI32(d, pos) == 0x54455532) { pos += 8; r.teu2 = true; }
  r.end = pos; return r;
}
static void layout(const char* what, bool header, int np, int takes, bool window, bool vst3, int numParams = 64)
{
  const std::vector<uint8_t> v = state(header, np, takes, window, vst3);
  const Read r = readState(v, numParams);
  const int want = (int)v.size() - (vst3 ? 4 : 0);   // the plugin's data ends before VST3's bypass
  char m[200]; snprintf(m, sizeof(m), "%s: %d parameters, %s, ends at %d (of %d)", what, r.np,
                        window ? (r.teu1 && r.teu2 ? "window's blocks found" : "window's blocks LOST") : "no window", r.end, want);
  CHECK(r.np == np && r.end == want && (!window || (r.teu1 && r.teu2)), m);
}
int main()
{
  printf("1.0.x projects (58 parameters, no header)\n");
  layout("AU", false, 58, 0, false, false);
  layout("VST3 (bypass after it)", false, 58, 0, false, true);
  layout("with takes stored", false, 58, 2, false, true);
  printf("1.1 test builds (no header)\n");
  layout("Twenty Engine UI (ui branch), 58, AU", false, 58, 0, true, false);
  layout("Twenty Engine UI, 63 (before VCA IN), AU", false, 63, 0, true, false);
  layout("Twenty Engine UI, 63, VST3", false, 63, 0, true, true);
  layout("Twenty Engine UI, 64, AU", false, 64, 0, true, false);
  layout("Twenty Engine UI, 64, VST3", false, 64, 0, true, true);
  layout("Twenty Engine MS-20ic, 64, VST3", false, 64, 0, false, true);
  printf("1.1 (header)\n");
  layout("UI, VST3", true, 64, 0, true, true);
  layout("MS-20ic, AU, takes stored", true, 64, 3, false, false);
  layout("a later version (70 parameters) reads a 1.1 state", true, 64, 0, true, true, 70);
  layout("1.1 reads a later state with more parameters", true, 70, 0, true, true);
  printf(fails ? "STATE CHECKS FAILED (%d)\n" : "ALL STATE CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
