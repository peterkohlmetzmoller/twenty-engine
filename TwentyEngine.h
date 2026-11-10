#pragma once

#include "IPlug_include_in_plug_hdr.h"
// Two plugins from one source: Twenty Engine (the instrument: plays like the hardware, keeps every take
// exactly as played) and Twenty Engine Studio (TWENTY_STUDIO 1, set in the Studio build's config.h:
// the knobs are automatable, so a performance becomes editable Logic automation).
#ifndef TWENTY_STUDIO
#define TWENTY_STUDIO 0
#endif
#include "engine/engine_gen.h"
#include "led_port.h"
#if TWENTY_STUDIO
#include "ui/StudioHost.h"
#include <string>
#endif

const int kNumPresets = 1;
const int kNumParams = ms20::NPARAMS;   // every parameter of the Ksoloti patch

#if IPLUG_DSP
#include "engine/ms20_host.h"
#include "engine/recall.h"
#include <memory>
#include <atomic>
#include <cstdio>
#include <cstdarg>
#endif

using namespace iplug;
#if IPLUG_EDITOR
using namespace igraphics;
#endif

class TwentyEngine final : public Plugin
#if TWENTY_STUDIO
  , public te_ui::PatchHost, public te_ui::MemoryHost
#endif
{
public:
  TwentyEngine(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset = -1) override;
  void OnIdle() override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  int RecallTakes() const { return mRecall.takes(); }      // UI: takes stored in this project
  bool RecallOn() const { return mLed.KnobsDirect(); }       // UI: recall needs the direct connection to the controller
  void DrainController();                                    // audio thread: everything straight from the controller
  void StudioDiag(char* out, int n) const;                   // Studio test build: what Logic does with the knobs
  void RecallClear() { mRecall.clear(); }                    // UI: forget them
#if TWENTY_STUDIO
  // ---- the editor window (ui/StudioUI.h); main thread
  int Controller() const { return mController.load(); }      // 0 none, 1 MS-20ic, 2 Ksoloti, 3 Teensy
  bool CableFlag(int which) const override { return mCableFlag[which & 1].load(); }
  void SetCableFlag(int which, bool on) override;
  int NoisePick(int j) const override { return (j >= 0 && j < 35) ? mNoise[j] : 0; }
  void SetNoisePick(int j, int pick) override { if (j >= 0 && j < 35) mNoise[j] = (uint8_t)pick; }
  void MemRecall(int n) override { mHost->memRecall(n); }
  void MemSave(int n) override { mHost->memSave(n); }
  bool MemValid(int n) const override { return mHost->memValid(n); }
  int MemSlot() const override { return mHost->memSlot(); }
  const char* MemName(int n) const override { return (n >= 0 && n < 36) ? mNames[n].c_str() : ""; }
  void SetMemName(int n, const char* name) override;
  bool MemImport(const char* path) override;
  bool MemExport(const char* path) override;
  void SwitchDown(bool down) override { UiMidi(0xD0, down ? 127 : 0, 0); }
#endif

private:
  static int32_t ToRaw(int idx, double value);     // plugin parameter value -> engine (Ksoloti) units
  static double FromRaw(int idx, int32_t raw);     // engine units -> plugin parameter value

  std::unique_ptr<ms20::MS20Host> mHost;   // the live engine: played straight from the controller, follows the panel
  std::unique_ptr<ms20::MS20Host> mPlay;   // the playback engine: plays the track; follows a recorded take or the panel
  ms20::Recall mRecall;         // performance recall (engine/recall.h)
  LedPort mLed;                 // sends the LED SysEx straight to the MS-20 iC (Core MIDI)
  bool mFromEngine = false;     // true while we push an engine-side change to the host
  int mRescan = 0;
  float mScratchL[512], mScratchR[512], mPlayL[512], mPlayR[512];
#if TWENTY_STUDIO
  // ---- editor window: MIDI the window sends to the engine (cable messages, the switch), cable states, memory names
  ms20::SpscQueue<uint32_t, 512> mUiMidi;
  void UiMidi(uint8_t s, uint8_t d1, uint8_t d2) { mUiMidi.push(((uint32_t)s << 16) | ((uint32_t)d1 << 8) | d2); }
  void UiCable(int type, int a, int b) { UiMidi(0xB0, 99, (uint8_t)type); UiMidi(0xB0, 98, (uint8_t)a); UiMidi(0xB0, 6, (uint8_t)b); }
  void WatchCable(uint8_t s, uint8_t d1, uint8_t d2);   // audio thread: cables patched on the MS-20ic (mono, tap)
  int mCw99 = -1, mCw98 = -1; bool mCwH99 = false, mCwH98 = false;
  std::atomic<bool> mCableFlag[2];
  uint8_t mNoise[35] = {};
  std::atomic<int> mController{0};
  int mCtlScan = 0;
  std::string mNames[36];
  char mMemDir[1024] = "";
  void LoadNames();
  void SaveNames();
#endif
  // Studio: the start of a new take is written as automation (every parameter touched once)
  std::atomic<bool> mTouchReq{false}, mRunningA{false};
  bool mTouchedThisRun = false, mWasRunning = false;
  int64_t mClock = 0, mLastRegionNote = -(int64_t)1 << 40;
  std::atomic<int> mDiagTouched{0}, mDiagHost{0}; std::atomic<uint64_t> mDiagHostBits{0};
  // ---- beta diagnostics: what touches the parameters around a memory recall
  struct DbgEv { int idx; int src; int32_t raw; };
  ms20::SpscQueue<DbgEv, 2048> mDbgQ;
  std::atomic<int> mDbgResets { 0 };
  struct DbgMidi { uint8_t s, d1, d2; int off; };
  ms20::SpscQueue<DbgMidi, 2048> mDbgMidiQ;
  FILE* mDbg = nullptr; int mDbgLines = 0; int mDbgSlot = -2; int mDbgResetsSeen = 0; int mDbgEngine = 0;
  void Dbg(const char* fmt, ...);
#endif
};
