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
  StatusControl(const IRECT& r, TwentyEngine* plug) : IControl(r), mPlug(plug) { Scan(); }

  bool IsDirty() override
  {
    if (++mFrames >= 30) { mFrames = 0; bool d = Scan(); const int t = Takes(); if (t != mTakes) { mTakes = t; d = true; } if (TWENTY_STUDIO) d = true; if (d) return true; }   // about twice a second
    return IControl::IsDirty();
  }

  // double-click on the takes line: forget the recorded takes (they are only the settings, the MIDI stays)
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mTakes > 0 && TakesRect().Contains(x, y)) { if (mPlug) mPlug->RecallClear(); mTakes = Takes(); SetDirty(false); }
  }

  void Draw(IGraphics& g) override
  {
    const IColor ink(255, 20, 20, 20), paper(255, 255, 255, 255), dim(255, 150, 150, 150);
    g.FillRect(paper, mRECT);
    g.DrawRect(ink, mRECT.GetPadded(-6.f), nullptr, 1.5f);
    const IRECT in = mRECT.GetPadded(-22.f);
#if TWENTY_STUDIO
    g.DrawText(IText(22.f, ink, "Roboto-Regular", EAlign::Near, EVAlign::Top), "TWENTY ENGINE STUDIO", in);
#else
    g.DrawText(IText(22.f, ink, "Roboto-Regular", EAlign::Near, EVAlign::Top), "TWENTY ENGINE", in);
#endif

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

    WDL_String tk;
#if TWENTY_STUDIO
    if (mPlug) { char d[200]; mPlug->StudioDiag(d, sizeof(d)); tk.Set(d); }   // test build: what Logic does with the knobs
    if (false)
#else
    if (mPlug && !mPlug->RecallOn())
#endif
#ifdef _WIN32
      tk.Set("Performance recall off: needs Windows 11 MIDI Services");   // the DAW holds the controller's port
#else
      tk.Set("Performance recall: connect the MS-20ic to this computer");
#endif
    else if (mTakes > 0) tk.SetFormatted(96, "Recorded performances: %d  \xC2\xB7  double-click to forget them", mTakes);
    else tk.Set("Recorded performances: none yet");
    g.DrawText(IText(11.f, dim, "Roboto-Regular", EAlign::Near, EVAlign::Middle), tk.Get(), TakesRect());
  }

private:
  bool Scan() { const int c = LedPort::FindController(); if (c == mController) return false; mController = c; return true; }
  int Takes() const { return mPlug ? mPlug->RecallTakes() : 0; }
  IRECT TakesRect() const { const IRECT in = mRECT.GetPadded(-22.f); return IRECT(in.L, in.T + 30.f, in.R, in.T + 46.f); }
  TwentyEngine* mPlug = nullptr;
  int mController = -1;
  int mTakes = -1;
  int mFrames = 0;
};
#endif

// parameter type helpers (types as written in the Ksoloti patch)
static bool IsFrac(int i) { return strncmp(kParamInfo[i].type, "frac32", 6) == 0; }
static bool IsBipolar(int i) { return strncmp(kParamInfo[i].type, "frac32.s", 8) == 0; }
static bool IsBool(int i) { return strncmp(kParamInfo[i].type, "bool32", 6) == 0; }

// The names a DAW shows (automation menus, generic editors): MS-20 panel names instead of the patch's object names.
// Keyed by the patch object and parameter, so a regenerated engine keeps them; anything unknown keeps its patch label.
struct NiceName { const char *object, *name, *label, *group; };
static const NiceName kNiceNames[] = {
  {"MS20_Voices", "vco1wave", "VCO 1 Waveform", "VCO"},        {"MS20_Voices", "vco1oct", "VCO 1 Scale", "VCO"},
  {"MS20_Voices", "pw", "VCO 1 Pulse Width", "VCO"},           {"MS20_Voices", "portamento", "Portamento", "VCO"},
  {"MS20_Voices", "multitrig", "Multi Trigger", "VCO"},        {"MS20_Voices", "vco2wave", "VCO 2 Waveform", "VCO"},
  {"MS20_Voices", "vco2oct", "VCO 2 Scale", "VCO"},            {"MS20_Voices", "vco2fine", "VCO 2 Fine Tune", "VCO"},
  {"MS20_Voices", "vco2semi", "VCO 2 Pitch", "VCO"},           {"MS20_Voices", "arpmode", "Play Mode", "Play Mode"},
  {"MS20_Voices", "arprate", "Arp Rate", "Play Mode"},         {"MS20_Voices", "arpoct", "Arp Octaves", "Play Mode"},
  {"MS20_Voices", "arporder", "Arp Order", "Play Mode"},
  {"EG1", "delay", "EG 1 Delay Time", "EG 1"},                 {"EG1", "attack", "EG 1 Attack Time", "EG 1"},
  {"EG1", "release", "EG 1 Release Time", "EG 1"},             {"EG1", "amount", "VCO EG 1 Amount", "EG 1"},
  {"EG2", "hold", "EG 2 Hold Time", "EG 2"},                   {"EG2", "attack", "EG 2 Attack Time", "EG 2"},
  {"EG2", "decay", "EG 2 Decay Time", "EG 2"},                 {"EG2", "sustain", "EG 2 Sustain Level", "EG 2"},
  {"EG2", "release", "EG 2 Release Time", "EG 2"},             {"EG2", "lpfdepth", "LPF EG 2 Amount", "Filters"},
  {"EG2", "hpfdepth", "HPF EG 2 Amount", "Filters"},
  {"Master_Mixer", "vco1", "VCO 1 Level", "Mixer"},            {"Master_Mixer", "vco2", "VCO 2 Level", "Mixer"},
  {"Master_Mixer", "extlvl", "External Level", "Mixer"},
  {"Filter_Macro", "hpfcut", "HPF Cutoff", "Filters"},         {"Filter_Macro", "hpfpeak", "HPF Peak", "Filters"},
  {"Filter_Macro", "lpfcut", "LPF Cutoff", "Filters"},         {"Filter_Macro", "lpfpeak", "LPF Peak", "Filters"},
  {"Filter_Macro", "clicknoise", "Key Click", "Filters"},
  {"VCA_Macro", "bassboost", "Bass Boost", "Output"},          {"VCA_Macro", "drive", "Drive", "Output"},
  {"VCA_Macro", "mastervol", "Volume", "Output"},
  {"PatchBay", "total", "Patch TOTAL", "Patch Panel"},         {"PatchBay", "freq", "Patch FREQ", "Patch Panel"},
  {"PatchBay", "vco2freq", "Patch VCO 2 CV", "Patch Panel"},   {"PatchBay", "hpf", "Patch HPF Cutoff", "Patch Panel"},
  {"PatchBay", "lpf", "Patch LPF Cutoff", "Patch Panel"},      {"PatchBay", "vca", "Patch VCA", "Patch Panel"},
  {"PatchBay", "eg1trig", "Patch EG 1 Trig", "Patch Panel"},   {"PatchBay", "eg2trig", "Patch EG 2 Trig", "Patch Panel"},
  {"PatchBay", "shin", "Patch S&H In", "Patch Panel"},         {"PatchBay", "shclock", "Patch S&H Clock", "Patch Panel"},
  {"MS20_Master_Tune", "tune", "Master Tune", "Output"},
  {"BBD_Delay", "time", "Delay Time", "Delay"},                {"BBD_Delay", "feedback", "Delay Feedback", "Delay"},
  {"BBD_Delay", "bbdfilter", "Delay Tone", "Delay"},           {"BBD_Delay", "wowflutter", "Delay Wow and Flutter", "Delay"},
  {"BBD_Delay", "drywet", "Delay Amount", "Delay"},            {"BBD_Delay", "spread", "Delay Spread", "Delay"},
  {"BBD_Delay", "width", "Delay Width", "Delay"},
  {"MG_Morphing_LFO", "rate", "MG Frequency", "MG"},           {"MG_Morphing_LFO", "shape", "MG Waveform", "MG"},
  {"MG_Morphing_LFO", "pitchmod", "VCO MG Amount", "MG"},      {"MG_Morphing_LFO", "lpfmod", "LPF MG Amount", "MG"},
  {"MG_Morphing_LFO", "hpfmod", "HPF MG Amount", "MG"},
};
static const NiceName *Nice(int i)
{
  for (const NiceName &n : kNiceNames)
    if (!strcmp(n.object, kParamInfo[i].object) && !strcmp(n.name, kParamInfo[i].name)) return &n;
  return nullptr;
}

TwentyEngine::TwentyEngine(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
#if IPLUG_DSP
  mHost = std::make_unique<MS20Host>();
  mPlay = std::make_unique<MS20Host>();
  mRecall.setEngines(mHost.get(), mPlay.get());
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
  // Not automatable: the sound is played on the controller and stored by performance recall, not edited in the DAW.
#if TWENTY_STUDIO
  const int kFlags = 0;   // Studio: every knob is automatable (Logic writes the performance as automation)
#else
  const int kFlags = IParam::kFlagCannotAutomate;
#endif
  for (int i = 0; i < kNumParams; i++)
  {
    const ParamInfo& p = kParamInfo[i];
    const NiceName *nn = Nice(i);
    const char *label = nn ? nn->label : p.label, *group = nn ? nn->group : p.object;
    if (IsFrac(i))
    {
      const double def = p.def / 2097152.0;          // 1<<21 per display unit
      if (IsBipolar(i)) GetParam(i)->InitDouble(label, def, -64.0, 64.0, 0.01, "", kFlags, group);
      else              GetParam(i)->InitDouble(label, def, 0.0, 64.0, 0.01, "", kFlags, group);
    }
    else if (IsBool(i))
      GetParam(i)->InitBool(label, p.def != 0, "", kFlags, group);
    else
    {
      const int maxv = (p.hmax > 0) ? p.hmax - 1 : 13;  // radio buttons; patch bay selectors 0..13
      GetParam(i)->InitInt(label, p.def, 0, maxv, "", kFlags, group);
    }
  }

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->LoadFont("Roboto-Regular", (void*)kRobotoTTF, (int)sizeof(kRobotoTTF));
    pGraphics->AttachControl(new StatusControl(pGraphics->GetBounds(), this));
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
  mPlay->reset(GetSampleRate());
  mRecall.resetAudio();
  // the reset keeps the engine's settings; make sure the plugin's current values are applied
  for (int i = 0; i < kNumParams; i++) mHost->setParamRaw(i, ToRaw(i, GetParam(i)->Value()));
}

void TwentyEngine::ProcessMidiMsg(const IMidiMsg& msg)
{
  // everything goes to the engine: notes, CCs (knobs, rotaries, patch bay NRPN), channel pressure (button)
  if (msg.mStatus < 0xF0 && (msg.mStatus & 0xF0) != 0xB0) mDbgMidiQ.push({msg.mStatus, msg.mData1, msg.mData2, msg.mOffset});   // beta diagnostics: notes, pressure (not the many knob CCs)
  // performance recall (engine/recall.h): the track plays the notes, the switch and the mod wheel; knob and
  // cable CCs on the track are ignored when the knobs reach the plugin straight from the controller
  if (!mLed.KnobsDirect()) { mHost->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2); return; }   // Windows: the track plays the live engine
  DrainController();                                                  // what the controller sent first: tells live from region
  const int type = msg.mStatus & 0xF0;
  if (type == 0xB0 && msg.mData1 != 1) return;                        // knob/cable data on the track: never used
  if (mRecall.trackIsLive(msg.mStatus, msg.mData1, msg.mData2)) return;   // the host's copy of live playing: the live engine has it
#if TWENTY_STUDIO
  // Studio: one engine plays the panel, the live playing and the regions; the knobs' performance is Logic automation
  if (type == 0x90 && msg.mData2 > 0) mLastRegionNote = mClock;
  mHost->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2);
  return;
#endif
  if (type == 0x90 && msg.mData2 > 0) mRecall.trackNoteOn(msg.mOffset, msg.mData1);
  mPlay->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2);      // a region: the playback engine
}

void TwentyEngine::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const double sr = GetSampleRate(), tempo = GetTempo();
  const bool direct = mLed.KnobsDirect();
  const bool running = GetTransportIsRunning();
  if (running && !mWasRunning) { mDiagHost.store(0); mDiagHostBits.store(0); }   // test counters: per playback
  if (!running) mTouchedThisRun = false;
  mWasRunning = running; mRunningA.store(running);
#if TWENTY_STUDIO
  mRecall.setEnabled(false);                    // Studio keeps the performance in Logic's automation
  const bool twoEngines = false;
#else
  mRecall.setEnabled(direct);
  const bool twoEngines = direct;
#endif
  if (direct) DrainController();
  mClock += nFrames;
  mRecall.beginBlock(GetTransportIsRunning(), GetPPQPos(), (sr > 0.0 && tempo > 0.0) ? tempo / 60.0 / sr : 0.0, nFrames);
  int done = 0;
  while (done < nFrames)
  {
    const int n = (nFrames - done > 512) ? 512 : nFrames - done;
    mRecall.setChunkOffset(done);
    mHost->process(mScratchL, mScratchR, n);
    if (twoEngines)
    {
      mPlay->process(mPlayL, mPlayR, n);
      for (int i = 0; i < n; i++) { mScratchL[i] += mPlayL[i]; mScratchR[i] += mPlayR[i]; }
    }
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
  if (!mFromEngine && source == kHost && mRunningA.load()) { mDiagHost++; mDiagHostBits.fetch_or(1ull << (paramIdx & 63)); }
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
#if TWENTY_STUDIO
    BeginInformHostOfParamChangeFromUI(p.idx);   // Studio: a knob move is a gesture Logic can write (Latch)
    SetParameterValue(p.idx, GetParam(p.idx)->ToNormalized(v));
    EndInformHostOfParamChangeFromUI(p.idx);
#else
    GetParam(p.idx)->Set(v);   // keep the plugin's copy in step (saved with the project); no automation gestures to the host
#endif
    mFromEngine = false;
  }
#if TWENTY_STUDIO
  // 1b. Studio: the start of a new take. Every parameter is touched once with its current value, so Logic
  //     (track automation in Latch) writes the complete starting sound into the automation lanes.
  if (mTouchReq.exchange(false))
  {
    mFromEngine = true;
    for (int i = 0; i < kNumParams; i++)
    {
      BeginInformHostOfParamChangeFromUI(i);
      SetParameterValue(i, GetParam(i)->GetNormalized());
      EndInformHostOfParamChangeFromUI(i);
    }
    mFromEngine = false;
    mDiagTouched.store(kNumParams);
  }
#endif
  // 2. patch memory: write the file after a save on the keys (never on the audio thread).
  //    While stored settings play back, replayed save gestures never overwrite the memory.
  mHost->pollMemorySave();
  { ParamSet q; while (mPlay->popEngineChange(q)) {} LedMsg l; while (mPlay->popLed(l)) {} mPlay->dropMemorySave(); }   // the playback engine: no LEDs, no saves
  // 2b. performance recall: store finished takes
  mRecall.idle();
  // 3. LEDs on the iC
  if (--mRescan <= 0) { mLed.Rescan(); mRescan = 100; }   // every ~2 s: finds the iC when it is plugged in later
  LedMsg m;
  while (mHost->popLed(m)) mLed.Send(m.b, m.len);
}
#endif

// ---- project state: the parameters (as before), then the recorded takes (performance recall) ----
bool TwentyEngine::SerializeState(IByteChunk& chunk) const
{
  if (!SerializeParams(chunk)) return false;
#if IPLUG_DSP
  const std::vector<uint8_t> b = mRecall.serialize();
  const int32_t n = (int32_t)b.size();
  chunk.Put(&n);
  if (n > 0) chunk.PutBytes(b.data(), n);
#endif
  return true;
}

int TwentyEngine::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int pos = UnserializeParams(chunk, startPos);
#if IPLUG_DSP
  // older projects (before recall) end here; anything that is not our block is left alone
  if (pos >= 0 && pos + 4 <= chunk.Size())
  {
    int32_t n = 0;
    const int p2 = chunk.Get(&n, pos);
    if (p2 > 0 && n > 0 && p2 + n <= chunk.Size())
    {
      std::vector<uint8_t> b((size_t)n);
      if (chunk.GetBytes(b.data(), n, p2) > 0 && mRecall.unserialize(b.data(), (size_t)n)) pos = p2 + n;
    }
  }
#endif
  return pos;
}


// Everything the controller sends, straight from it (Mac): it plays the live engine, like the hardware,
// and tells recall what is played live. The Ksoloti and Teensy editions send the memory keys to the
// computer as CC 102 (down) / 103 (up) with the key as value: here they are keys again.
void TwentyEngine::DrainController()
{
#if IPLUG_DSP
  uint8_t s, d1, d2;
  while (mLed.PopKnob(s, d1, d2))
  {
    if ((s & 0xF0) == 0xB0 && (d1 == 102 || d1 == 103)) { s = (uint8_t)((d1 == 102 ? 0x90 : 0x80) | (s & 0x0F)); d1 = d2; d2 = (s & 0xF0) == 0x90 ? 100 : 0; }
    const bool late = mRecall.directMsg(s, d1, d2);   // the host's copy of this key came first
#if TWENTY_STUDIO
    if (late) continue;                                // one engine: it already plays that key
    // the first key of a new take (the host runs, no region is playing): write the whole start as automation
    if ((s & 0xF0) == 0x90 && d2 > 0 && mRunningA.load() && !mTouchedThisRun && mClock - mLastRegionNote > (int64_t)(2.0 * GetSampleRate()))
    { mTouchedThisRun = true; mTouchReq.store(true); }
#else
    if (late) mPlay->midi(0, (uint8_t)(0x80 | (s & 0x0F)), d1, 0);   // silence the host's copy on the playback engine
#endif
    mHost->midi(0, s, d1, d2);
  }
#endif
}

void TwentyEngine::StudioDiag(char* out, int n) const
{
#if IPLUG_DSP
  int k = 0; uint64_t b = mDiagHostBits.load(); while (b) { k += (int)(b & 1); b >>= 1; }
  snprintf(out, n, "Studio test \xC2\xB7 start written: %d params \xC2\xB7 from Logic since play: %d values, %d params",
           mDiagTouched.load(), mDiagHost.load(), k);
#else
  if (n > 0) out[0] = 0;
#endif
}
