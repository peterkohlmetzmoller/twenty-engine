#include "TwentyEngine.h"
#include "IPlug_include_in_plug_src.h"
#if IPLUG_EDITOR && !defined(NO_IGRAPHICS)
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


#if IPLUG_EDITOR && !defined(NO_IGRAPHICS)
// The whole window (Mac, IGraphics): product name and whether a controller is plugged in. Windows: GDI, below.
class StatusControl : public IControl
{
public:
  StatusControl(const IRECT& r, TwentyEngine* plug) : IControl(r), mPlug(plug) { Scan(); }

  bool IsDirty() override
  {
    if (++mFrames >= 30) { mFrames = 0; bool d = Scan(); const int t = Takes(); if (t != mTakes) { mTakes = t; d = true; } const int ps = mPlug ? mPlug->PanelSource() : 0; if (ps != mPanel) { mPanel = ps; d = true; } if (TWENTY_STUDIO) d = true; if (d) return true; }   // about twice a second
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
    g.DrawText(IText(22.f, ink, "Roboto-Regular", EAlign::Near, EVAlign::Top), "TWENTY ENGINE AUTOMATION", in);
#else
    g.DrawText(IText(22.f, ink, "Roboto-Regular", EAlign::Near, EVAlign::Top), "TWENTY ENGINE INSTRUMENT", in);
#endif

    static const char* kNames[] = { "", "MS-20ic", "Ksoloti Core", "Teensy" };
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
    { char d[160]; d[0] = 0; if (mPlug) mPlug->StudioDiag(d, sizeof(d)); tk.Set(d); }   // test build: does the host report recording?
#else
    if (mPlug && !mPlug->RecallOn())
#ifdef _WIN32
      tk.Set("Performance recall off: needs Windows 11 MIDI Services");   // the DAW holds the controller's port
#else
      tk.Set("Performance recall: connect the MS-20ic to this computer");
#endif
    else if (mTakes > 0) tk.SetFormatted(96, "Recorded performances: %d  \xC2\xB7  double-click to forget them", mTakes);
    else tk.Set("Recorded performances: none yet");
#endif
    g.DrawText(IText(11.f, dim, "Roboto-Regular", EAlign::Near, EVAlign::Middle), tk.Get(), TakesRect());

    // where the live sound's panel came from (engine/panel_memory.h): the controller's own dump, or the copy kept here
    if (on)
    {
      const char* ptxt = mPanel == 2 ? "Panel read from the MS-20ic"
                       : mPanel == 1 ? "Panel from memory  \xC2\xB7  volume off/on reads the knobs"
                                     : "Switch the volume off and on to read the knobs";
      g.DrawText(IText(11.f, dim, "Roboto-Regular", EAlign::Near, EVAlign::Middle), ptxt, IRECT(in.L, in.T + 48.f, in.R, in.T + 64.f));
    }
  }

private:
  bool Scan() { const int c = LedPort::FindController(); if (c == mController) return false; mController = c; return true; }
  int Takes() const { return mPlug ? mPlug->RecallTakes() : 0; }
  IRECT TakesRect() const { const IRECT in = mRECT.GetPadded(-22.f); return IRECT(in.L, in.T + 30.f, in.R, in.T + 46.f); }
  TwentyEngine* mPlug = nullptr;
  int mController = -1;
  int mTakes = -1;
  int mPanel = -1;
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
#if TWENTY_STUDIO
  mHost->setHook(nullptr); mPlay->setHook(nullptr);   // Automation: the playback engine plays the region's stream
  for (int i = 0; i < kNumParams; i++)
    mStream.kind[i] = !strncmp(kParamInfo[i].type, "frac32.s", 8) ? ms20::ParamStream<kNumParams>::BI
                    : !strncmp(kParamInfo[i].type, "frac32", 6)   ? ms20::ParamStream<kNumParams>::UNI : ms20::ParamStream<kNumParams>::INTV;
#endif
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
      snprintf(mPanelPath, sizeof(mPanelPath), "%sMS20PANEL.BIN", dir);
      /* beta diagnostics log (off): char lp[1100]; snprintf(lp, sizeof(lp), "%sTwentyEngine-debug.txt", dir); mDbg = fopen(lp, "w"); */
    }
#else
    const char *home = getenv("HOME");
    if (home) {
      snprintf(dir, sizeof(dir), "%s/Library/Application Support/Twenty Engine/", home);
      char parent[1024]; snprintf(parent, sizeof(parent), "%s/Library/Application Support", home);
      MakeDir(parent); MakeDir(dir);
      mHost->loadMemories(dir);
      snprintf(mPanelPath, sizeof(mPanelPath), "%sMS20PANEL.BIN", dir);
      /* beta diagnostics log (off): char lp[1100]; snprintf(lp, sizeof(lp), "%sTwentyEngine-debug.txt", dir); mDbg = fopen(lp, "w"); */
    }
#endif
  }
#endif
#if IPLUG_DSP
#if TWENTY_STUDIO
  PanelPort::Get();   // the plugin's own MIDI port exists from the start, so the DAW sees it before recording
#endif
  if (mPanelPath[0] && mPanelMem.load(mPanelPath)) mPanelSrc.store(1);   // the last panel: replayed when the controller connects
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

#if IPLUG_EDITOR && !defined(NO_IGRAPHICS)
  mMakeGraphicsFunc = [&]() -> IGraphics* {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->LoadFont("Roboto-Regular", (void*)kRobotoTTF, (int)sizeof(kRobotoTTF));
    pGraphics->AttachControl(new StatusControl(pGraphics->GetBounds(), this));
  };
#endif
}

#if IPLUG_EDITOR && defined(_WIN32)
// ---- Windows: the status window with GDI (same content as StatusControl, no OpenGL) ----
#if TWENTY_STUDIO
static const char* kStatusClass = "TwentyEngineAutomationStatus";
#else
static const char* kStatusClass = "TwentyEngineInstrumentStatus";
#endif
static HINSTANCE StatusModule()
{
  HMODULE m = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&StatusModule, &m);
  return (HINSTANCE)m;
}
static void DrawUtf8(HDC dc, const char* s, RECT r, UINT fmt)
{
  wchar_t w[256];
  if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 256) > 0) DrawTextW(dc, w, -1, &r, fmt | DT_NOPREFIX);
}
static HFONT StatusFont(int px) { return CreateFontA(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI"); }

void* TwentyEngine::OpenWindow(void* pParent)
{
  HINSTANCE hi = StatusModule();
  WNDCLASSA have;
  if (!GetClassInfoA(hi, kStatusClass, &have))
  {
    WNDCLASSA wc = {};
    wc.style = CS_DBLCLKS; wc.lpfnWndProc = &TwentyEngine::StatusWndProc; wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = kStatusClass;
    RegisterClassA(&wc);
  }
  mStatusWnd = CreateWindowExA(0, kStatusClass, "", WS_CHILD | WS_VISIBLE, 0, 0, PLUG_WIDTH, PLUG_HEIGHT, (HWND)pParent, nullptr, hi, nullptr);
  if (mStatusWnd) { SetWindowLongPtrA(mStatusWnd, GWLP_USERDATA, (LONG_PTR)this); SetTimer(mStatusWnd, 1, 500, nullptr); }
  return mStatusWnd;
}

void TwentyEngine::CloseWindow()
{
  if (!mStatusWnd) return;
  KillTimer(mStatusWnd, 1);
  SetWindowLongPtrA(mStatusWnd, GWLP_USERDATA, 0);
  DestroyWindow(mStatusWnd);
  mStatusWnd = nullptr;
  UnregisterClassA(kStatusClass, StatusModule());   // a DLL's classes outlive it otherwise (fails harmlessly while other windows exist)
}

LRESULT CALLBACK TwentyEngine::StatusWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
  TwentyEngine* p = (TwentyEngine*)GetWindowLongPtrA(h, GWLP_USERDATA);
  switch (msg)
  {
    case WM_TIMER: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDBLCLK:   // double-click on the takes line: forget the recorded takes
    {
      const int y = (short)HIWORD(lp);
      if (p && y >= 50 && y <= 70 && p->RecallTakes() > 0) { p->RecallClear(); InvalidateRect(h, nullptr, FALSE); }
      return 0;
    }
    case WM_PAINT:
    {
      PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
      if (p) p->PaintStatus(dc);
      EndPaint(h, &ps);
      return 0;
    }
  }
  return DefWindowProcA(h, msg, wp, lp);
}

void TwentyEngine::PaintStatus(HDC wdc)
{
  const int W = PLUG_WIDTH, H = PLUG_HEIGHT;
  HDC dc = CreateCompatibleDC(wdc);
  HBITMAP bm = CreateCompatibleBitmap(wdc, W, H);
  HGDIOBJ oldBm = SelectObject(dc, bm);
  const COLORREF ink = RGB(20, 20, 20), paper = RGB(255, 255, 255), dim = RGB(150, 150, 150);
  RECT all = {0, 0, W, H};
  HBRUSH bPaper = CreateSolidBrush(paper); FillRect(dc, &all, bPaper); DeleteObject(bPaper);
  HPEN pen = CreatePen(PS_SOLID, 2, ink); HGDIOBJ oldPen = SelectObject(dc, pen);
  HGDIOBJ oldBr = SelectObject(dc, GetStockObject(NULL_BRUSH));
  Rectangle(dc, 6, 6, W - 6, H - 6);
  SetBkMode(dc, TRANSPARENT);
  const int L = 22, R = W - 22, T = 22, B = H - 22;

  HFONT fTitle = StatusFont(22), fRow = StatusFont(14), fSmall = StatusFont(12);
  HGDIOBJ oldF = SelectObject(dc, fTitle);
  SetTextColor(dc, ink);
#if TWENTY_STUDIO
  DrawUtf8(dc, "TWENTY ENGINE AUTOMATION", RECT{L, T - 4, R, T + 24}, DT_LEFT | DT_TOP | DT_SINGLELINE);
#else
  DrawUtf8(dc, "TWENTY ENGINE INSTRUMENT", RECT{L, T - 4, R, T + 24}, DT_LEFT | DT_TOP | DT_SINGLELINE);
#endif

  static const char* kNames[] = { "", "MS-20ic", "Ksoloti Core", "Teensy" };
  const int controller = LedPort::FindController();
  const bool on = controller > 0;
  // connection row at the bottom
  const int cy = B - 9;
  HBRUSH bInk = CreateSolidBrush(ink);
  if (on) { HGDIOBJ ob = SelectObject(dc, bInk); HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, L + 1, cy - 5, L + 12, cy + 6); SelectObject(dc, op); SelectObject(dc, ob); }
  else { HPEN pd = CreatePen(PS_SOLID, 1, dim); HGDIOBJ op = SelectObject(dc, pd); Ellipse(dc, L + 1, cy - 5, L + 12, cy + 6); SelectObject(dc, op); DeleteObject(pd); }
  DeleteObject(bInk);
  char txt[96];
  if (on) snprintf(txt, sizeof(txt), "Connected  \xC2\xB7  %s", kNames[controller]); else snprintf(txt, sizeof(txt), "Not connected");
  SelectObject(dc, fRow); SetTextColor(dc, on ? ink : dim);
  DrawUtf8(dc, txt, RECT{L + 20, B - 18, R, B}, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  // takes line (Instrument) or the test line (Automation)
  char tk[192];
#if TWENTY_STUDIO
  tk[0] = 0; StudioDiag(tk, sizeof(tk));
#else
  const int takes = RecallTakes();
  if (!RecallOn()) snprintf(tk, sizeof(tk), "Performance recall off: needs Windows 11 MIDI Services");
  else if (takes > 0) snprintf(tk, sizeof(tk), "Recorded performances: %d  \xC2\xB7  double-click to forget them", takes);
  else snprintf(tk, sizeof(tk), "Recorded performances: none yet");
#endif
  SelectObject(dc, fSmall); SetTextColor(dc, dim);
  DrawUtf8(dc, tk, RECT{L, T + 30, R, T + 46}, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  if (on)
  {
    const int ps = PanelSource();
    const char* ptxt = ps == 2 ? "Panel read from the MS-20ic"
                     : ps == 1 ? "Panel from memory  \xC2\xB7  volume off/on reads the knobs"
                               : "Switch the volume off and on to read the knobs";
    DrawUtf8(dc, ptxt, RECT{L, T + 48, R, T + 64}, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  }

  SelectObject(dc, oldF); DeleteObject(fTitle); DeleteObject(fRow); DeleteObject(fSmall);
  SelectObject(dc, oldBr); SelectObject(dc, oldPen); DeleteObject(pen);
  BitBlt(wdc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
  SelectObject(dc, oldBm); DeleteObject(bm); DeleteDC(dc);
}
#endif

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
#if TWENTY_STUDIO
  // Automation: the plugin's values (and any DAW automation) go to the playback engine; the live engine keeps
  // the panel (the controller's last panel, panel_memory.h); with one engine it plays them too.
  for (int i = 0; i < kNumParams; i++)
  {
    const int32_t raw = ToRaw(i, GetParam(i)->Value());
    mPlay->setParamRaw(i, raw);
    if (!mLed.KnobsDirect()) mHost->setParamRaw(i, raw);
  }
#else
  for (int i = 0; i < kNumParams; i++) mHost->setParamRaw(i, ToRaw(i, GetParam(i)->Value()));
#endif
}

void TwentyEngine::ProcessMidiMsg(const IMidiMsg& msg)
{
  // everything goes to the engine: notes, CCs (knobs, rotaries, patch bay NRPN), channel pressure (button)
  if (msg.mStatus < 0xF0 && (msg.mStatus & 0xF0) != 0xB0) mDbgMidiQ.push({msg.mStatus, msg.mData1, msg.mData2, msg.mOffset});   // beta diagnostics: notes, pressure (not the many knob CCs)
  // performance recall (engine/recall.h): the track plays the notes, the switch and the mod wheel; knob and
  // cable CCs on the track are ignored when the knobs reach the plugin straight from the controller
#if TWENTY_STUDIO
  // Automation: the region's settings (channels 16/15, engine/paramstream.h), applied per block in ProcessBlock -
  // always read, also where this computer has no port of its own (a project recorded elsewhere keeps its sounds)
  if (mStream.push(msg.mStatus, msg.mData1, msg.mData2)) return;
#endif
  if (!mLed.KnobsDirect()) { mHost->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2); return; }   // Windows: the track plays the live engine
  DrainController();                                                  // what the controller sent first: tells live from region
  const int type = msg.mStatus & 0xF0;
#if TWENTY_STUDIO
  // Automation without its own port (Windows for now): the region's knob CCs play the playback engine, like any
  // MIDI instrument. With the port, the region's stream carries the sound and the MS-20ic's CCs are ignored.
  if (type == 0xB0 && msg.mData1 != 1 && PanelPort::Get().Ok()) return;
  if (type == 0xB0 && msg.mData1 != 1) { if (!mRecall.trackIsLive(msg.mStatus, msg.mData1, msg.mData2)) mPlay->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2); return; }
#else
  if (type == 0xB0 && msg.mData1 != 1) return;                        // knob/cable data on the track: never used
#endif
  if (mRecall.trackIsLive(msg.mStatus, msg.mData1, msg.mData2)) return;   // the host's copy of live playing: the live engine has it
  if (type == 0x90 && msg.mData2 > 0) mRecall.trackNoteOn(msg.mOffset, msg.mData1);
  mPlay->midi(msg.mOffset, msg.mStatus, msg.mData1, msg.mData2);      // a region: the playback engine
}

void TwentyEngine::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const double sr = GetSampleRate(), tempo = GetTempo();
  const bool direct = mLed.KnobsDirect();
  const bool running = GetTransportIsRunning();
  if (running && !mWasRunning) { mDiagHost.store(0); mDiagHostBits.store(0); mStreamApplied.store(0); mSnapReq.store(true); }   // Automation: the whole panel goes out at every start
  mWasRunning = running; mRunningA.store(running);
  mRecall.setEnabled(TWENTY_STUDIO ? false : direct);   // Automation keeps the performance in the host's automation
  const bool twoEngines = direct;
  // the controller has just connected (or the plugin just started with it): play the last panel into the live
  // engine as a start-up dump - the sound matches the knobs and the start-up light show runs
  if (direct && !mWasDirect) mPanelMem.replay([this](uint8_t st, uint8_t d1, uint8_t d2) { mHost->midi(0, st, d1, d2); });
  mWasDirect = direct;
  if (direct) DrainController();
#if TWENTY_STUDIO
  // Automation: the region's settings that arrived for this block, onto the engine that plays the track
  mStream.flush([this](int slot, int32_t raw) { StreamApply(slot, raw); });
  if (running) mStream.runningBlock(); else mStream.stoppedBlock();
#endif
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
  const int32_t raw = ToRaw(paramIdx, GetParam(paramIdx)->Value());
#if TWENTY_STUDIO
  // Automation: the host's values (DAW automation, a loaded project) play on the playback engine; with one engine
  // (no direct connection to the controller) on the live engine too.
  mPlay->setParamRaw(paramIdx, raw);
  if (!mLed.KnobsDirect()) mHost->setParamRaw(paramIdx, raw);
#else
  mHost->setParamRaw(paramIdx, raw);
#endif
}

#if TWENTY_STUDIO
void TwentyEngine::StreamSend(int slot, int32_t raw)
{
  mStream.encode(slot, raw, [](uint8_t st, uint8_t d1, uint8_t d2) { const uint8_t m[3] = {st, d1, d2}; PanelPort::Get().Send(m, 3); });
  mStreamSent++;
}

void TwentyEngine::StreamApply(int slot, int32_t raw)
{
  // audio thread, before the engines run: the engine that plays the track (playback engine, or the only one)
  ms20::MS20Host* e = mLed.KnobsDirect() ? mPlay.get() : mHost.get();
  mStreamApplied++;
  if (slot < kNumParams) { e->setParamQuiet(slot, raw); return; }
  // the mono and tap cables are cable events in the engine: replay the cable
  const bool mono = slot == ms20::ParamStream<kNumParams>::SLOT_MONO;
  e->midi(0, 0xB0, 99, raw ? 4 : 5); e->midi(0, 0xB0, 98, (uint8_t)(mono ? 31 : 33)); e->midi(0, 0xB0, 6, (uint8_t)(mono ? 25 : 3));
}
#endif

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
#if TWENTY_STUDIO
  // 1a. Automation: the transport has started: the whole panel through the port, behind a new snapshot number.
  //     A recording track records it (a take starts with its sound); otherwise it is only echoed back and ignored.
  const bool port = PanelPort::Get().Ok();
  const int64_t nowMs = ms20::ParamStream<kNumParams>::NowMs();
  const bool again = mRunningA.load() && nowMs - mLastSnapMs >= ms20::ParamStream<kNumParams>::SNAP_EVERY_MS;
  if ((mSnapReq.exchange(false) || again) && port)
  {
    mLastSnapMs = nowMs;
    mStream.encodeMark([](uint8_t st, uint8_t d1, uint8_t d2) { const uint8_t m[3] = {st, d1, d2}; PanelPort::Get().Send(m, 3); });
    for (int i = 0; i < kNumParams; i++) StreamSend(i, mHost->getParamRaw(i));
    StreamSend(ms20::ParamStream<kNumParams>::SLOT_MONO, mStreamMono);
    StreamSend(ms20::ParamStream<kNumParams>::SLOT_TAP, mStreamTap);
  }
#endif
  // 1. changes made by the engine (iC knobs via CC, power-on dump, ESP knob layers, tap tempo, patch bay)
  ParamSet p;
  while (mHost->popEngineChange(p))
  {
    mDbgEngine++;
    if (p.idx < 0 || p.idx >= kNumParams) continue;
#if TWENTY_STUDIO
    // Automation: the panel changed: out through the port (a recording track records it)
    if (port) StreamSend(p.idx, p.raw);
#else
    mFromEngine = true;
    GetParam(p.idx)->Set(FromRaw(p.idx, p.raw));   // keep the plugin's copy in step (saved with the project); no automation gestures to the host
    mFromEngine = false;
#endif
  }
  // 1c. the controller's panel: a real start-up dump (volume switched on) makes it "read"; saved after a quiet second
  { const uint32_t pons = mPanelMem.powerOns(); if (pons != mPanelPons) { mPanelPons = pons; mPanelSrc.store(2); }
    const uint32_t v = mPanelMem.changed();
    if (v != mPanelSeenVer) { mPanelSeenVer = v; mPanelQuiet = 0; }
    else if (v != mPanelSavedVer && ++mPanelQuiet >= 50 && mPanelPath[0]) { if (mPanelMem.save(mPanelPath)) mPanelSavedVer = v; mPanelQuiet = 0; } }
#if TWENTY_STUDIO
  // 1d. Automation: the mono and tap cables (cable events, not settings) from the controller's panel
  if (port)
  {
    ms20::PanelMemory::State st;
    if (mPanelMem.snapshot(st))
    {
      bool m = false, t = false;
      for (int k = 0; k < st.ncab; k++)
      {
        const int a = st.cab[k][0], b = st.cab[k][1];
        if ((a == 31 && b == 25) || (a == 25 && b == 31)) m = true;
        if ((a == 33 && b == 3) || (a == 3 && b == 33)) t = true;
      }
      if (m != mStreamMono) { mStreamMono = m; StreamSend(ms20::ParamStream<kNumParams>::SLOT_MONO, m); }
      if (t != mStreamTap) { mStreamTap = t; StreamSend(ms20::ParamStream<kNumParams>::SLOT_TAP, t); }
    }
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
    mPanelMem.onMessage(s, d1, d2);                    // keep the panel (MS20PANEL.BIN)
    if ((s & 0xF0) == 0xB0 && (d1 == 102 || d1 == 103)) { s = (uint8_t)((d1 == 102 ? 0x90 : 0x80) | (s & 0x0F)); d1 = d2; d2 = (s & 0xF0) == 0x90 ? 100 : 0; }
    const bool late = mRecall.directMsg(s, d1, d2);   // the host's copy of this key came first
    if (late) mPlay->midi(0, (uint8_t)(0x80 | (s & 0x0F)), d1, 0);   // silence the host's copy on the playback engine
    mHost->midi(0, s, d1, d2);
  }
#endif
}

void TwentyEngine::StudioDiag(char* out, int n) const
{
#if IPLUG_DSP
  if (!PanelPort::Get().Ok()) snprintf(out, n, "No MIDI port: the region's knob CCs play (as a MIDI instrument)");
  else snprintf(out, n, "Sound in the region \xC2\xB7 sent %d \xC2\xB7 from the region since play: %d", mStreamSent.load(), mStreamApplied.load());
#else
  if (n > 0) out[0] = 0;
#endif
}
