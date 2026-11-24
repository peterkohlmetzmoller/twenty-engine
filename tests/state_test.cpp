// state_test.cpp - every saved-state layout we have shipped or tested is read with the right parameter count
// (engine/state_format.h): 1.0.x (58 parameters, AU and VST3), 1.1 test builds (63/64, with the window's block),
// 1.1 with its header, takes stored, and a 1.1 state read by a later version with more parameters.
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
  put32(v, takes); for (int i = 0; i < takes; i++) v.push_back((uint8_t)(i * 7));
  if (window) { put32(v, 0x54455531); put32(v, 1); put32(v, 0); for (int i = 0; i < 35; i++) v.push_back(0); put32(v, 0x54455532); put32(v, 1); }
  if (vst3) put32(v, 0);                                                    // bypass
  return v;
}
static int count(const std::vector<uint8_t>& v, int numParams, int* at) { return StateParamCount(v.data(), (int)v.size(), 0, numParams, at); }
int main()
{
  int at = -1;
  printf("1.0.x projects (58 parameters, no header)\n");
  CHECK(count(state(false, 58, 0, false, false), 64, &at) == 58 && at == 0, "AU: 58");
  CHECK(count(state(false, 58, 0, false, true), 64, &at) == 58, "VST3 (bypass after it): 58");
  CHECK(count(state(false, 58, 40, false, true), 64, &at) == 58, "with takes stored: 58");
  printf("1.1 test builds (no header)\n");
  CHECK(count(state(false, 64, 0, true, false), 64, &at) == 64, "Twenty Engine UI, 64, AU");
  CHECK(count(state(false, 64, 0, true, true), 64, &at) == 64, "Twenty Engine UI, 64, VST3");
  CHECK(count(state(false, 63, 0, true, true), 64, &at) == 63, "Twenty Engine UI, 63 (before VCA IN)");
  CHECK(count(state(false, 64, 0, false, true), 64, &at) == 64, "Twenty Engine MS-20ic, 64, VST3");
  printf("1.1 (header)\n");
  CHECK(count(state(true, 64, 0, true, true), 64, &at) == 64 && at == 8, "header: 64, parameters after it");
  CHECK(count(state(true, 64, 0, false, false), 70, &at) == 64 && at == 8, "a later version (70 parameters) reads a 1.1 state: 64");
  CHECK(count(state(true, 70, 0, true, true), 64, &at) == 70, "1.1 reads a later state with more parameters: 70 (the rest skipped)");
  printf(fails ? "STATE CHECKS FAILED (%d)\n" : "ALL STATE CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
