// Twenty Engine for Linux: the sound side of the app (DPF plugin class, run by DPF's standalone host).
// The same engine as everywhere (engine/ms20_host.h, generated from the Ksoloti patch). The MS-20ic is read
// straight through ALSA (led_port.h), like the Mac plugin reads it through Core MIDI: its knobs, cables, keys and
// switch play the live engine, the LEDs go back to it, and its last panel is kept (MS20PANEL.BIN). Other MIDI
// keyboards play the keys. The window (ui/StudioUI.h) turns the same parameters and patches the same cables.
#pragma once
#include "DistrhoPlugin.hpp"
#include "engine/engine_gen.h"
#include "engine/ms20_host.h"
#include "engine/panel_memory.h"
#include "led_port.h"
#include "ui/StudioHost.h"
#include <atomic>
#include <memory>
#include <thread>

START_NAMESPACE_DISTRHO

static const int kNumParams = ms20::NPARAMS;

class TwentyEnginePlugin : public Plugin, public te_ui::PatchHost
{
public:
  TwentyEnginePlugin();
  ~TwentyEnginePlugin() override;

  // ---- for the window (main thread)
  int Controller() const { return mController.load(); }               // 0 none, 1 MS-20ic, 2 Ksoloti, 3 Teensy
  int32_t EngineRaw(int i) const { return mHost->getParamRaw(i); }    // what the engine plays now (knobs follow it)
  int64_t TouchedMs(int i) const { return mTouchMs[i].load(); }        // when the window last turned it
  int PanelSource() const { return mPanelSrc.load(); }
  bool CableFlag(int which) const override { return mCableFlag[which & 1].load(); }
  void SetCableFlag(int which, bool on) override;
  int NoisePick(int j) const override { return (j >= 0 && j < 35) ? mNoise[j] : 0; }
  void SetNoisePick(int j, int pick) override { if (j >= 0 && j < 35) mNoise[j] = (uint8_t)pick; }
  void SwitchDown(bool down) override { UiMidi(0xD0, down ? 127 : 0, 0); }
  int Leds() const override { return mUiLeds.load(); }
  static int32_t ToRaw(int idx, double value);
  static double FromRaw(int idx, int32_t raw);
  static void ParamRange(int idx, int& type, double& def, double& mn, double& mx);   // type 0 knob, 1 selector, 2 switch
  static int64_t NowMs();

protected:
  const char* getLabel() const override { return "TwentyEngine"; }
  const char* getDescription() const override { return "8-voice polysynth for the Korg MS-20ic"; }
  const char* getMaker() const override { return "Peter Kohlmetz Moller"; }
  const char* getLicense() const override { return "GPL-3.0-or-later"; }
  uint32_t getVersion() const override { return d_version(1, 0, 0); }
  int64_t getUniqueId() const override { return d_cconst('T', 'w', '2', '0'); }
  void initParameter(uint32_t index, Parameter& p) override;
  float getParameterValue(uint32_t index) const override;
  void setParameterValue(uint32_t index, float value) override;
  void activate() override;
  void sampleRateChanged(double sr) override;
  void run(const float**, float** outputs, uint32_t frames, const MidiEvent*, uint32_t) override;

private:
  void Idle();                                   // the app's idle thread, every 20 ms
  void UiMidi(uint8_t s, uint8_t d1, uint8_t d2) { mUiMidi.push(((uint32_t)s << 16) | ((uint32_t)d1 << 8) | d2); }
  void UiCable(int type, int a, int b) { UiMidi(0xB0, 99, (uint8_t)type); UiMidi(0xB0, 98, (uint8_t)a); UiMidi(0xB0, 6, (uint8_t)b); }
  void WatchCable(uint8_t s, uint8_t d1, uint8_t d2);

  std::unique_ptr<ms20::MS20Host> mHost;
  LedPort mLed;
  ms20::PanelMemory mPanelMem;
  char mPanelPath[1100] = "";
  bool mWasDirect = false;
  std::atomic<int> mPanelSrc{0}, mController{0}, mUiLeds{0x10};
  uint32_t mPanelSeenVer = 0, mPanelSavedVer = 0, mPanelPons = 0; int mPanelQuiet = 0;
  ms20::SpscQueue<uint32_t, 512> mUiMidi;
  int mCw99 = -1, mCw98 = -1; bool mCwH99 = false, mCwH98 = false;
  std::atomic<bool> mCableFlag[2] = {false, false};
  uint8_t mNoise[35] = {};
  std::atomic<int64_t> mTouchMs[ms20::NPARAMS] = {};
  float mL[512], mR[512];
  std::atomic<bool> mRun{true};
  std::thread mIdle;

  DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TwentyEnginePlugin)
};

END_NAMESPACE_DISTRHO
