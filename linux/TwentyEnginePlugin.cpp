// Twenty Engine for Linux: the sound side (see TwentyEnginePlugin.hpp). The same steps as TwentyEngine.cpp
// (the Mac/Windows plugin) where the controller is read directly: DrainController, the panel memory, the LEDs.
#include "TwentyEnginePlugin.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

START_NAMESPACE_DISTRHO

using namespace ms20;

static bool IsFrac(int i) { return strncmp(kParamInfo[i].type, "frac32", 6) == 0; }
static bool IsBipolar(int i) { return strncmp(kParamInfo[i].type, "frac32.s", 8) == 0; }
static bool IsBool(int i) { return strncmp(kParamInfo[i].type, "bool32", 6) == 0; }

int64_t TwentyEnginePlugin::NowMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// the same ranges as the Mac/Windows plugin (and the Ksoloti editor): knobs 0..64 (bipolar -64..64), selectors as integers
void TwentyEnginePlugin::ParamRange(int i, int& type, double& def, double& mn, double& mx)
{
  const ParamInfo& p = kParamInfo[i];
  if (IsFrac(i)) { type = 0; def = p.def / 2097152.0; mn = IsBipolar(i) ? -64.0 : 0.0; mx = 64.0; }
  else if (IsBool(i)) { type = 2; def = p.def != 0; mn = 0.0; mx = 1.0; }
  else { type = 1; def = p.def; mn = 0.0; mx = (p.hmax > 0) ? p.hmax - 1 : 13; }
}

int32_t TwentyEnginePlugin::ToRaw(int idx, double value)
{
  if (IsFrac(idx))
  {
    double r = value * 2097152.0;
    if (r > 134217727.0) r = 134217727.0;
    if (r < -134217728.0) r = -134217728.0;
    return (int32_t)lrint(r);
  }
  return (int32_t)lrint(value);
}

double TwentyEnginePlugin::FromRaw(int idx, int32_t raw) { return IsFrac(idx) ? raw / 2097152.0 : (double)raw; }

TwentyEnginePlugin::TwentyEnginePlugin()
: Plugin(kNumParams, 0, 0)
{
  mHost = std::make_unique<MS20Host>();
  // patch memory (36 sounds on the keys) and the controller's last panel: ~/.config/twenty-engine/
  {
    char dir[1024] = "";
    const char* xdg = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    if (xdg && *xdg) snprintf(dir, sizeof(dir), "%s/twenty-engine/", xdg);
    else if (home) { char c[1024]; snprintf(c, sizeof(c), "%s/.config", home); mkdir(c, 0755); snprintf(dir, sizeof(dir), "%s/.config/twenty-engine/", home); }
    if (dir[0])
    {
      mkdir(dir, 0755);
      mHost->loadMemories(dir);
      snprintf(mPanelPath, sizeof(mPanelPath), "%sMS20PANEL.BIN", dir);
    }
  }
  if (mPanelPath[0] && mPanelMem.load(mPanelPath)) mPanelSrc.store(1);
  mHost->reset(getSampleRate() > 0 ? getSampleRate() : 48000.0);
  mIdle = std::thread([this] { while (mRun.load()) { Idle(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); } });
}

TwentyEnginePlugin::~TwentyEnginePlugin()
{
  mRun.store(false);
  if (mIdle.joinable()) mIdle.join();
  if (mPanelPath[0] && mPanelMem.changed() != mPanelSavedVer) mPanelMem.save(mPanelPath);
}

void TwentyEnginePlugin::initParameter(uint32_t index, Parameter& p)
{
  const ParamInfo& pi = kParamInfo[index];
  int type; double def, mn, mx; ParamRange((int)index, type, def, mn, mx);
  p.hints = kParameterIsAutomatable | (type == 1 ? kParameterIsInteger : 0) | (type == 2 ? (kParameterIsBoolean | kParameterIsInteger) : 0);
  p.name = pi.label;
  String sym(pi.object); sym += "_"; sym += pi.name;
  p.symbol = sym;
  p.ranges.def = (float)def; p.ranges.min = (float)mn; p.ranges.max = (float)mx;
}

// what the engine plays now (the MS-20ic moves these)
float TwentyEnginePlugin::getParameterValue(uint32_t index) const { return (float)FromRaw((int)index, mHost->getParamRaw((int)index)); }

// the window (or a host): straight to the live engine, as the MS-20ic's knob would
void TwentyEnginePlugin::setParameterValue(uint32_t index, float value)
{
  if (index >= (uint32_t)kNumParams) return;
  mTouchMs[index].store(NowMs());
  mHost->setParamRaw((int)index, ToRaw((int)index, value));
}

void TwentyEnginePlugin::activate() { mHost->reset(getSampleRate()); }
void TwentyEnginePlugin::sampleRateChanged(double sr) { mHost->reset(sr); }

void TwentyEnginePlugin::run(const float**, float** outputs, uint32_t frames, const MidiEvent*, uint32_t)
{
  // (the host's MIDI events are not used: the same keyboards reach the app through its ALSA client)
  const bool direct = mLed.KnobsDirect();
  // the controller has just connected (or the app just started with it): its last panel as a start-up dump,
  // so the sound matches the knobs and the start-up light show runs
  if (direct && !mWasDirect) mPanelMem.replay([this](uint8_t st, uint8_t d1, uint8_t d2) { mHost->midi(0, st, d1, d2); });
  mWasDirect = direct;
  uint8_t s, d1, d2;
  // the controller: everything plays the live engine (the Ksoloti/Teensy editions send the memory keys as CC 102/103)
  while (mLed.PopKnob(s, d1, d2))
  {
    mPanelMem.onMessage(s, d1, d2);
    if ((s & 0xF0) == 0xB0 && (d1 == 102 || d1 == 103)) { s = (uint8_t)((d1 == 102 ? 0x90 : 0x80) | (s & 0x0F)); d1 = d2; d2 = (s & 0xF0) == 0x90 ? 100 : 0; }
    WatchCable(s, d1, d2);
    mHost->midi(0, s, d1, d2);
  }
  // other keyboards: keys, wheels, pedals - never the switch (channel pressure would hold it: silent keys)
  while (mLed.PopKey(s, d1, d2))
  {
    if ((s & 0xF0) == 0xD0) continue;
    mHost->midi(0, s, d1, d2);
  }
  { uint32_t m; while (mUiMidi.pop(m)) mHost->midi(0, (uint8_t)(m >> 16), (uint8_t)(m >> 8), (uint8_t)m); }
  uint32_t done = 0;
  while (done < frames)
  {
    const uint32_t n = (frames - done > 512) ? 512 : frames - done;
    mHost->process(mL, mR, (int)n);
    memcpy(outputs[0] + done, mL, n * sizeof(float));
    memcpy(outputs[1] + done, mR, n * sizeof(float));
    done += n;
  }
}

// the idle thread: patch memory file, the controller (connect, LEDs), the panel file
void TwentyEnginePlugin::Idle()
{
  static int rescan = 0, scan = 0;
  mHost->pollMemorySave();
  { ParamSet p; while (mHost->popEngineChange(p)) {} }   // the window reads the engine itself
  if (--rescan <= 0) { mLed.Rescan(); rescan = 100; }   // every 2 s: the controller plugged in later
  if (--scan <= 0) { mController.store(LedPort::FindController()); scan = 50; }
  LedMsg m;
  while (mHost->popLed(m))
  {
    mLed.Send(m.b, m.len);
    // the window's LEDs: F0 42 40 6E 01 01 <led> <3F on / 1F off> F7
    if (m.len >= 8 && m.b[0] == 0xF0 && m.b[5] == 0x01 && m.b[6] < 5)
    { const int bit = 1 << m.b[6]; int v = mUiLeds.load(); v = (m.b[7] == 0x3F) ? (v | bit) : (v & ~bit); mUiLeds.store(v); }
  }
  // the controller's panel: a real start-up dump makes it "read"; saved after a quiet second
  const uint32_t pons = mPanelMem.powerOns(); if (pons != mPanelPons) { mPanelPons = pons; mPanelSrc.store(2); }
  const uint32_t v = mPanelMem.changed();
  if (v != mPanelSeenVer) { mPanelSeenVer = v; mPanelQuiet = 0; }
  else if (v != mPanelSavedVer && ++mPanelQuiet >= 50 && mPanelPath[0]) { if (mPanelMem.save(mPanelPath)) mPanelSavedVer = v; mPanelQuiet = 0; }
}

// the two cables that are not parameters: KBD CV -> VCO 1+2 CV IN (mono) and SWITCH -> ESP SIGNAL IN (tap tempo),
// sent as the MS-20ic's own cable messages
void TwentyEnginePlugin::SetCableFlag(int which, bool on)
{
  which &= 1;
  const int a = which ? 33 : 31, b = which ? 3 : 25;
  mCableFlag[which].store(on);
  if (on) UiCable(4, a, b);
  else { UiCable(5, a, b); UiCable(5, b, b); UiCable(5, a, a); }
}

// audio thread: follow the MS-20ic's cable messages for the window
void TwentyEnginePlugin::WatchCable(uint8_t s, uint8_t d1, uint8_t d2)
{
  if ((s & 0xF0) != 0xB0) return;
  if (d1 == 99) { mCw99 = d2; mCwH99 = true; mCwH98 = false; return; }
  if (d1 == 98) { mCw98 = d2; mCwH98 = true; return; }
  if (d1 != 6 || !mCwH99 || !mCwH98) return;
  mCwH99 = mCwH98 = false;
  const int t = mCw99, a = mCw98, b = d2;
  for (int w = 0; w < 2; w++)
  {
    const int o = w ? 33 : 31, i = w ? 3 : 25;
    if ((a == o && b == i) || (a == i && b == o)) { if (t == 4) mCableFlag[w].store(true); else if (t == 5) mCableFlag[w].store(false); }
    if (t == 5 && a == b && (a == o || a == i)) mCableFlag[w].store(false);
  }
}

Plugin* createPlugin() { return new TwentyEnginePlugin(); }

END_NAMESPACE_DISTRHO
