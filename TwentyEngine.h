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

const int kNumPresets = 1;
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
  std::atomic<bool> mSnapReq{false};
  bool mStreamMono = false, mStreamTap = false;
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
  struct DbgMidi { uint8_t s, d1, d2; int off; };
  ms20::SpscQueue<DbgMidi, 2048> mDbgMidiQ;
  FILE* mDbg = nullptr; int mDbgLines = 0; int mDbgSlot = -2; int mDbgResetsSeen = 0; int mDbgEngine = 0;
  void Dbg(const char* fmt, ...);
#endif
};
