#pragma once

#include "IPlug_include_in_plug_hdr.h"
// Twenty Engine: the live engine plays from the MS-20ic; the sound is recorded into the region as MIDI
// (engine/paramstream.h) and a second engine plays the regions with it (TWENTY_STUDIO 1, config.h).
// TWENTY_STUDIO 0 is the older "Instrument" (performance recall), kept for reference.
#ifndef TWENTY_STUDIO
#define TWENTY_STUDIO 0
#endif
#include "engine/engine_gen.h"
#include "led_port.h"
#if TWENTY_UI
#include "ui/StudioHost.h"
#endif

#if TWENTY_UI
const int kNumPresets = 37;   // the 36 memories on the keys + the panel, in the host's preset menu
#else
const int kNumPresets = 1;
#endif
const int kNumParams = ms20::NPARAMS;   // every parameter of the Ksoloti patch

#if IPLUG_DSP
#include "engine/ms20_host.h"
#include "engine/recall.h"
#include "engine/paramstream.h"
#include "engine/panel_memory.h"
#include <memory>
#include <atomic>
#include <cstdio>
#include <cstdarg>
#endif

using namespace iplug;
#if IPLUG_EDITOR && !defined(NO_IGRAPHICS)
using namespace igraphics;
#endif

class TwentyEngine final : public Plugin
#if TWENTY_UI
  , public te_ui::PatchHost
#endif
{
public:
  TwentyEngine(const InstanceInfo& info);
  ~TwentyEngine();

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
  // Several tracks with the plugin: only one copy plays from the controller at a time - the one on the track
  // the host sends the live playing to (the selected / armed track). It shows itself by receiving, through its
  // track, the notes the controller just sent; the other copies play only their regions.
  bool HasFocus() const;
  bool DirectNow() const { return mLed.KnobsDirect(); }
#if TWENTY_UI
  // ---- the panel window (ui/StudioUI.h); main thread
  int Controller() const { return mController.load(); }      // 0 none, 1 MS-20ic, 2 Ksoloti, 3 Teensy
  bool CableFlag(int which) const override { return mCableFlag[which & 1].load(); }
  void SetCableFlag(int which, bool on) override;
  int NoisePick(int j) const override { return (j >= 0 && j < 35) ? mNoise[j] : 0; }
  void SetNoisePick(int j, int pick) override { if (j >= 0 && j < 35) mNoise[j] = (uint8_t)pick; }
  void SwitchDown(bool down) override { UiMidi(0xD0, down ? 127 : 0, 0); }
  int Leds() const override { return mUiLeds.load(); }
  int ArpSync() const override { return mArpSync.load(); }
  void SetArpSync(int v) override { mArpSync.store(v > 0 ? 1 : 0); }
#endif
  int PanelSource() const { return mPanelSrc.load(); }       // UI: 0 unknown, 1 from memory (MS20PANEL.BIN), 2 read from the controller
#if IPLUG_EDITOR && defined(_WIN32)
  // Windows: the window is drawn with plain GDI instead of OpenGL (some graphics drivers, e.g. in virtual
  // machines, crashed the OpenGL window)
  void* OpenWindow(void* pParent) override;
  void CloseWindow() override;
  void PaintStatus(HDC dc);
  static LRESULT CALLBACK StatusWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
  HWND mStatusWnd = nullptr;
#endif

private:
#if TWENTY_UI
  // ---- panel window: MIDI it sends to the live engine (cable messages, the switch), cable states, held knobs
  ms20::SpscQueue<uint32_t, 512> mUiMidi;
  void UiMidi(uint8_t s, uint8_t d1, uint8_t d2) { mUiMidi.push(((uint32_t)s << 16) | ((uint32_t)d1 << 8) | d2); }
  void UiCable(int type, int a, int b) { UiMidi(0xB0, 99, (uint8_t)type); UiMidi(0xB0, 98, (uint8_t)a); UiMidi(0xB0, 6, (uint8_t)b); }
  void WatchCable(uint8_t s, uint8_t d1, uint8_t d2);   // audio thread: cables patched on the MS-20ic (mono, tap)
  int mCw99 = -1, mCw98 = -1; bool mCwH99 = false, mCwH98 = false;
  std::atomic<bool> mCableFlag[2] = {false, false};
  uint8_t mNoise[35] = {};
  std::atomic<int> mController{0};
  static constexpr int32_t kMemPresetMagic = 0x54454D53;   // 'TEMS': a preset that is a memory slot
  int mPresetShown = 36;
  std::atomic<int> mArpSync{0};   // arp SYNC switch: 1 = on the DAW's beat, the RATE knob picks the division
  double mArpPpq = 0.0;
  int mCtlScan = 0;
  std::atomic<int64_t> mUiTouchMs[kNumParams] = {};
  int32_t mUiShown[kNumParams] = {};
  int64_t mHostSetMs[kNumParams] = {}, mUiClkSeen = -1;
  std::atomic<int> mUiLeds{0x10};
#endif
  int mId = 0;
  int64_t mFocusId = 0;
  bool mHadFocus = false;
  struct Heard { uint8_t key; int64_t ms; };
  Heard mHeard[16] = {}; int mHeardHead = 0;     // controller note-ons while another track has the controller
  uint8_t mHeldDirect[128] = {};                 // keys held on the live engine (channel + 1), released on hand-over
  void Claim(const IMidiMsg& msg);
  bool HeardKey(uint8_t key) const;
  static int32_t ToRaw(int idx, double value);     // plugin parameter value -> engine (Ksoloti) units
  static double FromRaw(int idx, int32_t raw);     // engine units -> plugin parameter value

  std::unique_ptr<ms20::MS20Host> mHost;   // the live engine: played straight from the controller, follows the panel
  std::unique_ptr<ms20::MS20Host> mPlay;   // the playback engine: plays the track; follows a recorded take or the panel
  ms20::Recall mRecall;         // performance recall (engine/recall.h)
  LedPort mLed;                 // sends the LED SysEx straight to the MS-20 iC (Core MIDI)
  bool mFromEngine = false;     // true while we push an engine-side change to the host
  int mRescan = 0;
  float mScratchL[512], mScratchR[512], mPlayL[512], mPlayR[512];
  std::atomic<bool> mRunningA{false};
  bool mWasRunning = false;
  int64_t mClock = 0;
  // Automation (engine/paramstream.h): the panel goes out through the plugin's own port and is recorded into the
  // region; the region's stream sets the playback engine
  ms20::ParamStream<kNumParams> mStream;
  std::unique_ptr<ms20::StreamMemory<kNumParams>> mMem;   // the regions' sound by song position (audio thread)
  std::atomic<bool> mStartReq{false}, mStreamEver{false}, mFollowA{false};
  int64_t mLastStreamClk = -1000000000; double mAheadPos = -1e9; bool mFollowing = false; int32_t mFollowSeen[kNumParams] = {};
  bool mRunBefore = false, mWasRunPpq = false; double mExpectPpq = 0.0; int mMemPend = 0; double mMemPendPos = 0.0; int mMemUsed = 0;
  std::atomic<bool> mSnapReq{false};
  bool mStreamMono = false, mStreamTap = false, mPanelMono = false, mPanelTap = false;
  int64_t mLastSnapMs = 0;
  std::atomic<int> mStreamSent{0}, mStreamApplied{0};
  void StreamSend(int slot, int32_t raw);    // main thread: one setting through the port
  void StreamApply(int slot, int32_t raw);   // audio thread: one setting from the region
  // the controller's last panel (engine/panel_memory.h): replayed as a start-up dump when the controller connects
  ms20::PanelMemory mPanelMem;
  char mPanelPath[1100] = "";
  bool mWasDirect = false;
  std::atomic<int> mPanelSrc{0};
  uint32_t mPanelSeenVer = 0, mPanelSavedVer = 0, mPanelPons = 0; int mPanelQuiet = 0;
  std::atomic<int> mDiagHost{0}; std::atomic<uint64_t> mDiagHostBits{0};
  // ---- beta diagnostics: what touches the parameters around a memory recall
  struct DbgEv { int idx; int src; int32_t raw; };
  ms20::SpscQueue<DbgEv, 2048> mDbgQ;
  std::atomic<int> mDbgResets { 0 };
  struct DbgMidi { uint8_t s, d1, d2; int off; int64_t clk = 0; };
  // diagnostics: the audio blocks right after the transport starts (or the host resets the plugin)
  struct DbgBlk { int64_t clk; double ppq; int frames, streamN, applied, resets, hostSets; uint8_t own, foreign, running; float peak; int memUsed, memN; };
  ms20::SpscQueue<DbgBlk, 1024> mDbgBlkQ;
  int mDbgBlocksLeft = 0, mDbgResetsAudio = 0; int64_t mDbgFocus = -1;
  std::atomic<int> mDbgHostSets{0};
  ms20::SpscQueue<DbgMidi, 2048> mDbgMidiQ;
  FILE* mDbg = nullptr; int mDbgLines = 0; int mDbgSlot = -2; int mDbgResetsSeen = 0; int mDbgEngine = 0;
  void Dbg(const char* fmt, ...);
#endif
};
