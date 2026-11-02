#include "TwentyEngine.h"
#include "IPlug_include_in_plug_src.h"
#if IPLUG_EDITOR
#include "IControls.h"
#include "resources/font_data.h"
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
static void MakeDir(const char* d) { _mkdir(d); }
#else
static void MakeDir(const char* d) { mkdir(d, 0755); }
#endif

using namespace ms20;

#if IPLUG_EDITOR
// The whole window: product name and whether a controller is plugged in. Nothing else.
class StatusControl : public IControl
{
public:
  StatusControl(const IRECT& r) : IControl(r) { mIgnoreMouse = true; Scan(); }

  bool IsDirty() override
  {
    if (++mFrames >= 30) { mFrames = 0; if (Scan()) return true; }   // look about twice a second
    return IControl::IsDirty();
  }

  void Draw(IGraphics& g) override
  {
    const IColor ink(255, 20, 20, 20), paper(255, 255, 255, 255), dim(255, 150, 150, 150);
    g.FillRect(paper, mRECT);
    g.DrawRect(ink, mRECT.GetPadded(-6.f), nullptr, 1.5f);
    const IRECT in = mRECT.GetPadded(-22.f);
    g.DrawText(IText(22.f, ink, "Roboto-Regular", EAlign::Near, EVAlign::Top), "TWENTY ENGINE", in);

    static const char* kNames[] = { "", "MS-20 iC", "Ksoloti Core", "Teensy" };
    const bool on = mController > 0;
    const IRECT row = in.GetFromBottom(18.f);
    const float cx = row.L + 6.f, cy = row.MH();
    if (on) g.FillCircle(ink, cx, cy, 5.f);
    else    g.DrawCircle(dim, cx, cy, 5.f, nullptr, 1.5f);
    WDL_String txt;
    if (on) txt.SetFormatted(64, "Connected  \xC2\xB7  %s", kNames[mController]);
    else    txt.Set("Not connected");
    g.DrawText(IText(14.f, on ? ink : dim, "Roboto-Regular", EAlign::Near, EVAlign::Middle), txt.Get(), row.GetHShifted(20.f));
  }

private:
  bool Scan() { const int c = LedPort::FindController(); if (c == mController) return false; mController = c; return true; }
  int mController = -1;
  int mFrames = 0;
};
#endif

// parameter type helpers (types as written in the Ksoloti patch)
static bool IsFrac(int i) { return strncmp(kParamInfo[i].type, "frac32", 6) == 0; }
static bool IsBipolar(int i) { return strncmp(kParamInfo[i].type, "frac32.s", 8) == 0; }
static bool IsBool(int i) { return strncmp(kParamInfo[i].type, "bool32", 6) == 0; }

TwentyEngine::TwentyEngine(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
#if IPLUG_DSP
  mHost = std::make_unique<MS20Host>();
  // Patch memory (36 sounds on the iC's keys) lives in one file, shared by every instance and project,
  // like the SD card in the hardware version. No file yet = factory sounds.
  //   Mac:     ~/Library/Application Support/Twenty Engine/MS20MEM.BIN
  //   Windows: %APPDATA%\Twenty Engine\MS20MEM.BIN
  {
    char dir[1024] = "";
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    if (base) {
      snprintf(dir, sizeof(dir), "%s/Twenty Engine/", base);
      MakeDir(dir);
      mHost->loadMemories(dir);
      /* beta diagnostics log (off): char lp[1100]; snprintf(lp, sizeof(lp), "%sTwentyEngine-debug.txt", dir); mDbg = fopen(lp, "w"); */
    }
#else
    const char *home = getenv("HOME");
    if (home) {
      snprintf(dir, sizeof(dir), "%s/Library/Application Support/Twenty Engine/", home);
      char parent[1024]; snprintf(parent, sizeof(parent), "%s/Library/Application Support", home);
      MakeDir(parent); MakeDir(dir);
      mHost->loadMemories(dir);
      /* beta diagnostics log (off): char lp[1100]; snprintf(lp, sizeof(lp), "%sTwentyEngine-debug.txt", dir); mDbg = fopen(lp, "w"); */
    }
#endif
  }
#endif
  // Same ranges as the Ksoloti editor shows them: knobs 0..64 (bipolar -64..64), selectors as integers.
  for (int i = 0; i < kNumParams; i++)
  {
    const ParamInfo& p = kParamInfo[i];
    if (IsFrac(i))
    {
      const double def = p.def / 2097152.0;          // 1<<21 per display unit
      if (IsBipolar(i)) GetParam(i)->InitDouble(p.label, def, -64.0, 64.0, 0.01, "", 0, p.object);
      else              GetParam(i)->InitDouble(p.label, def, 0.0, 64.0, 0.01, "", 0, p.object);
    }
    else if (IsBool(i))
      GetParam(i)->InitBool(p.label, p.def != 0, "", 0, p.object);
    else
    {
      const int maxv = (p.hmax > 0) ? p.hmax - 1 : 13;  // radio buttons; patch bay selectors 0..13
      GetParam(i)->InitInt(p.label, p.def, 0, maxv, "", 0, p.object);
    }
  }

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->LoadFont("Roboto-Regular", (void*)kRobotoTTF, (int)sizeof(kRobotoTTF));
    pGraphics->AttachControl(new StatusControl(pGraphics->GetBounds()));
  };
#endif
}

#if IPLUG_DSP
int32_t TwentyEngine::ToRaw(int idx, double value)
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

double TwentyEngine::FromRaw(int idx, int32_t raw)
{
  if (IsFrac(idx)) return raw / 2097152.0;
  return (double)raw;
}

void TwentyEngine::Dbg(const char* fmt, ...)
{
  if (!mDbg || mDbgLines > 4000) return;
  va_list a; va_start(a, fmt); vfprintf(mDbg, fmt, a); va_end(a); fputc('\n', mDbg); fflush(mDbg); mDbgLines++;
}

void TwentyEngine::OnReset()
{
  mDbgResets++;
  mHost->reset(GetSampleRate());
  // the reset keeps the engine's settings; make sure the plugin's current values are applied
  for (int i = 0; i < kNumParams; i++) mHost->setParamRaw(i, ToRaw(i, GetParam(i)->Value()));
}

void TwentyEngine::ProcessMidiMsg(const IMidiMsg& msg)
{
  // everything goes to the engine: notes, CCs (knobs, rotaries, patch bay NRPN), channel pressure (button)
  if (msg.mStatus < 0xF0 && (msg.mStatus & 0xF0) != 0xB0) mDbgMidiQ.push({msg.mStatus, msg.mData1, msg.mData2, msg.mOffset});   // beta diagnostics: notes, pressure (not the many knob CCs)
  mHost->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2);
}

void TwentyEngine::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  int done = 0;
  while (done < nFrames)
  {
    const int n = (nFrames - done > 512) ? 512 : nFrames - done;
    mHost->process(mScratchL, mScratchR, n);
    for (int i = 0; i < n; i++)
    {
      outputs[0][done + i] = (sample)mScratchL[i];
      outputs[1][done + i] = (sample)mScratchR[i];
    }
    done += n;
  }
}

void TwentyEngine::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)
{
  if (!mFromEngine) mDbgQ.push({paramIdx, (int)source, ToRaw(paramIdx, GetParam(paramIdx)->Value())});
  if (mFromEngine) return;   // the engine already has this value (it made the change)
  mHost->setParamRaw(paramIdx, ToRaw(paramIdx, GetParam(paramIdx)->Value()));
}

void TwentyEngine::OnIdle()
{
  // main thread, about every 20 ms
  // 0. beta diagnostics
  { int slot = mHost->memSlot(); if (slot != mDbgSlot) { Dbg("MEMORY slot %d (-1 = panel)  LPF cut %.2f  VCO1 wave %d  PW %.2f", slot,
      mHost->getParamRaw(PARAM_INDEX_Filter__Macro_lpfcut) / 2097152.0, (int)mHost->getParamRaw(PARAM_INDEX_MS20__Voices_vco1wave), mHost->getParamRaw(PARAM_INDEX_MS20__Voices_pw) / 2097152.0); mDbgSlot = slot; }
    int r = mDbgResets.load(); if (r != mDbgResetsSeen) { Dbg("RESET from the host (%d so far)", r); mDbgResetsSeen = r; }
    DbgMidi m; while (mDbgMidiQ.pop(m)) { const char* k = (m.s & 0xF0) == 0x90 ? (m.d2 ? "NOTE ON " : "NOTE ON v0") : (m.s & 0xF0) == 0x80 ? "NOTE OFF" : (m.s & 0xF0) == 0xD0 ? "PRESSURE" : (m.s & 0xF0) == 0xA0 ? "POLY AT " : "OTHER   ";
      Dbg("MIDI in %s  status %02X  data %3d %3d  (offset %d)", k, m.s, m.d1, m.d2, m.off); }
    DbgEv e; while (mDbgQ.pop(e)) Dbg("HOST sets param %d '%s' = %.2f  (source %d)", e.idx, kParamInfo[e.idx].label, e.raw / 2097152.0, e.src); }
  // 1. changes made by the engine (iC knobs via CC, power-on dump, ESP knob layers, tap tempo, patch bay)
  ParamSet p;
  while (mHost->popEngineChange(p))
  {
    mDbgEngine++;
    if (p.idx < 0 || p.idx >= kNumParams) continue;
    const double v = FromRaw(p.idx, p.raw);
    mFromEngine = true;
    BeginInformHostOfParamChangeFromUI(p.idx);
    SetParameterValue(p.idx, GetParam(p.idx)->ToNormalized(v));
    EndInformHostOfParamChangeFromUI(p.idx);
    mFromEngine = false;
  }
  // 2. patch memory: write the file after a save on the keys (never on the audio thread)
  mHost->pollMemorySave();
  // 3. LEDs on the iC
  if (--mRescan <= 0) { mLed.Rescan(); mRescan = 100; }   // every ~2 s: finds the iC when it is plugged in later
  LedMsg m;
  while (mHost->popLed(m)) mLed.Send(m.b, m.len);
}
#endif
