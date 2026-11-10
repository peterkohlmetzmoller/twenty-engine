// StudioUI.h - the editor window of Twenty Engine Studio.
// The MS-20 layout, drawn in our own style (no Korg branding): knob sections on the left, the patch panel on the
// right, the 36 memory keys along the bottom. Everything is vector-drawn, so the window scales cleanly.
//  - Knobs and switches are the plugin's parameters: the DAW records and automates them, and an MS-20ic moves them.
//  - The ESP knobs carry three layers (DELAY / TONE / ARP); a switch picks the layer, the labels follow it.
//  - Patch cables: drag from a jack to a jack. Click a cable's plug at an input to pull it out (drag it elsewhere to re-patch).
//    Inputs are the PatchBay parameters, so cables are saved, recalled and mirrored from the MS-20ic like any knob.
//    VCO 1+2 CV IN (mono) and ESP SIGNAL IN (tap tempo) are cable states, sent to the engine as the MS-20ic's own
//    cable messages. Jacks the engine does not use yet are drawn greyed out.
//  - Memory keys: click = recall, SAVE then a key = store, double-click = rename. IMPORT / EXPORT move a whole
//    bank (MS20MEM.BIN, the same file the Ksoloti SD card and the Teensy use).
#pragma once
#include "IControls.h"
#include "StudioHost.h"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace te_ui {
using namespace iplug;
using namespace iplug::igraphics;
using namespace ms20;

// ---------------------------------------------------------------- look
static const IColor kBg(255, 27, 27, 29), kCard(255, 37, 37, 40), kCardHi(255, 46, 46, 50), kInk(255, 236, 233, 226),
  kDim(255, 140, 137, 132), kFaint(255, 78, 77, 76), kOr(255, 224, 85, 43), kOrDim(255, 120, 52, 32), kKnob(255, 18, 18, 19),
  kWhiteKey(255, 222, 219, 212), kBlackKey(255, 24, 24, 26);
static const char* kFont = "Roboto-Regular";
static IText T(float sz, const IColor& c, EAlign a = EAlign::Center, EVAlign v = EVAlign::Middle) { return IText(sz, c, kFont, a, v); }
static float Rad(float deg) { return deg * 3.14159265f / 180.f; }

// section card with an orange title
class Section : public IControl
{
public:
  Section(const IRECT& r, const char* title) : IControl(r), mTitle(title) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(kCard, mRECT, 6.f);
    g.DrawText(T(12.f, kOr, EAlign::Near), mTitle.Get(), mRECT.GetFromTop(20.f).GetHShifted(10.f));
  }
private:
  WDL_String mTitle;
};

// ---------------------------------------------------------------- knob
// value arc in orange (from the centre for bipolar knobs), pointer, name above, MS-20 name or value below
class Knob : public IKnobControlBase
{
public:
  Knob(const IRECT& r, int param, const char* label, const char* sub = "", std::vector<std::string> zones = {})
  : IKnobControlBase(r, param, EDirection::Vertical, 2.0), mLabel(label), mSub(sub), mZones(zones) {}
  void SetLabels(const char* label, const char* sub) { mLabel.Set(label); mSub.Set(sub); SetDirty(false); }
  void Draw(IGraphics& g) override
  {
    const IRECT lab = mRECT.GetFromTop(14.f);
    g.DrawText(T(11.5f, IsDisabled() ? kFaint : kInk), mLabel.Get(), lab);
    const float cx = mRECT.MW(), cy = mRECT.T + 14.f + 25.f, r = 19.f;
    const double v = GetValue();
    const IParam* p = GetParam();
    const bool bip = p && p->GetMin() < 0.0;
    g.DrawArc(kFaint, cx, cy, r + 4.f, -135.f, 135.f, nullptr, 2.5f);
    const float a = -135.f + 270.f * (float)v;
    if (bip) { if (a >= 0.f) g.DrawArc(kOr, cx, cy, r + 4.f, 0.f, a, nullptr, 2.5f); else g.DrawArc(kOr, cx, cy, r + 4.f, a, 0.f, nullptr, 2.5f); }
    else if (v > 0.002) g.DrawArc(kOr, cx, cy, r + 4.f, -135.f, a, nullptr, 2.5f);
    g.FillCircle(mMouseIsOver || mMouseDown ? kCardHi : kKnob, cx, cy, r);
    g.DrawCircle(kFaint, cx, cy, r, nullptr, 1.f);
    g.DrawLine(kInk, cx + std::sin(Rad(a)) * r * 0.35f, cy - std::cos(Rad(a)) * r * 0.35f, cx + std::sin(Rad(a)) * r * 0.9f, cy - std::cos(Rad(a)) * r * 0.9f, nullptr, 2.2f);
    // zone names around the knob (play mode, MG waveform)
    for (size_t i = 0; i < mZones.size(); i++)
    {
      const float za = -135.f + 270.f * (float)i / (float)(mZones.size() - 1);
      const float zx = cx + std::sin(Rad(za)) * (r + 15.f), zy = cy - std::cos(Rad(za)) * (r + 13.f);
      g.DrawText(T(8.5f, kDim), mZones[i].c_str(), IRECT(zx - 22.f, zy - 6.f, zx + 22.f, zy + 6.f));
    }
    const IRECT below = mRECT.GetFromBottom(14.f);
    if ((mMouseDown || mMouseIsOver) && p)
    {
      WDL_String s; s.SetFormatted(32, bip ? "%+.1f" : "%.1f", p->Value());
      g.DrawText(T(10.5f, kOr), s.Get(), below);
    }
    else g.DrawText(T(9.f, kDim), mSub.Get(), below);
  }
  void OnMouseOver(float x, float y, const IMouseMod& m) override { IKnobControlBase::OnMouseOver(x, y, m); SetDirty(false); }
  void OnMouseOut() override { IKnobControlBase::OnMouseOut(); SetDirty(false); }
private:
  WDL_String mLabel, mSub;
  std::vector<std::string> mZones;
};

// ---------------------------------------------------------------- rotary switch (4 positions)
class Selector : public IKnobControlBase
{
public:
  Selector(const IRECT& r, int param, const char* label, std::vector<std::string> names)
  : IKnobControlBase(r, param, EDirection::Vertical, 1.0), mLabel(label), mNames(names) {}
  void Draw(IGraphics& g) override
  {
    g.DrawText(T(11.5f, kInk), mLabel.Get(), mRECT.GetFromTop(14.f));
    const IParam* p = GetParam();
    const int n = (int)mNames.size(), sel = p ? (int)std::lround(p->Value()) : 0;
    const float cx = mRECT.MW(), cy = mRECT.T + 14.f + 25.f, r = 13.f;
    for (int i = 0; i < n; i++)
    {
      const float a = -60.f + 120.f * (float)i / (float)(n - 1);
      const float tx = cx + std::sin(Rad(a)) * 27.f, ty = cy - std::cos(Rad(a)) * 22.f;
      g.FillCircle(i == sel ? kOr : kFaint, cx + std::sin(Rad(a)) * (r + 4.f), cy - std::cos(Rad(a)) * (r + 4.f), 1.8f);
      (void)tx; (void)ty;
    }
    const float a = -60.f + 120.f * (float)sel / (float)(n > 1 ? n - 1 : 1);
    g.FillCircle(mMouseIsOver || mMouseDown ? kCardHi : kKnob, cx, cy, r);
    g.DrawCircle(kFaint, cx, cy, r, nullptr, 1.f);
    g.DrawLine(kInk, cx, cy, cx + std::sin(Rad(a)) * r * 0.9f, cy - std::cos(Rad(a)) * r * 0.9f, nullptr, 2.2f);
    if (sel >= 0 && sel < n) g.DrawText(T(10.5f, kOr), mNames[sel].c_str(), mRECT.GetFromBottom(14.f));
  }
  void OnMouseDown(float x, float y, const IMouseMod& m) override { mDragged = false; IKnobControlBase::OnMouseDown(x, y, m); }
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& m) override { mDragged = true; IKnobControlBase::OnMouseDrag(x, y, dX, dY, m); }
  void OnMouseUp(float x, float y, const IMouseMod& m) override
  {
    IKnobControlBase::OnMouseUp(x, y, m);
    if (!mDragged && GetParam())   // a click steps to the next position
    {
      const int n = (int)mNames.size(), sel = (int)std::lround(GetParam()->Value());
      SetValue(GetParam()->ToNormalized((double)((sel + 1) % n)));
      GetDelegate()->BeginInformHostOfParamChangeFromUI(GetParamIdx());
      SetDirty(true);
      GetDelegate()->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }
  }
  void OnMouseOver(float x, float y, const IMouseMod& m) override { IKnobControlBase::OnMouseOver(x, y, m); SetDirty(false); }
  void OnMouseOut() override { IKnobControlBase::OnMouseOut(); SetDirty(false); }
private:
  WDL_String mLabel; std::vector<std::string> mNames; bool mDragged = false;
};

// ---------------------------------------------------------------- ESP layers
struct EspLayer { const char* name; int params[4]; const char* labels[4]; };
static const char* kEspHardware[4] = { "LOW CUT", "HIGH CUT", "CV ADJUST", "THRESHOLD" };
static const EspLayer kEspLayers[3] = {
  { "DELAY", { PARAM_INDEX_BBD__Delay_time, PARAM_INDEX_BBD__Delay_feedback, PARAM_INDEX_BBD__Delay_wowflutter, PARAM_INDEX_BBD__Delay_drywet },
             { "Time", "Feedback", "Wow", "Amount" } },
  { "TONE",  { PARAM_INDEX_BBD__Delay_bbdfilter, PARAM_INDEX_BBD__Delay_spread, PARAM_INDEX_BBD__Delay_width, PARAM_INDEX_BBD__Delay_drywet },
             { "Tone", "Spread", "Width", "Amount" } },
  { "ARP",   { PARAM_INDEX_MS20__Voices_arprate, PARAM_INDEX_MS20__Voices_arpoct, PARAM_INDEX_MS20__Voices_arporder, PARAM_INDEX_BBD__Delay_drywet },
             { "Rate", "Octaves", "Order", "Amount" } },
};

// three-way switch; follows the play mode (poly -> DELAY, arp/latch -> ARP) until it is clicked
class EspSwitch : public IControl
{
public:
  EspSwitch(const IRECT& r, std::function<void(int)> onLayer) : IControl(r), mOn(onLayer) {}
  int Layer() const { return mLayer; }
  void SetLayer(int l) { if (l == mLayer) return; mLayer = l; if (mOn) mOn(l); SetDirty(false); }
  void Draw(IGraphics& g) override
  {
    const float w = mRECT.W() / 3.f;
    for (int i = 0; i < 3; i++)
    {
      const IRECT b(mRECT.L + i * w + 1.f, mRECT.T, mRECT.L + (i + 1) * w - 1.f, mRECT.B);
      g.FillRoundRect(i == mLayer ? kOr : kKnob, b, 3.f);
      g.DrawText(T(10.5f, i == mLayer ? kBg : kDim), kEspLayers[i].name, b);
    }
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override { SetLayer(std::max(0, std::min(2, (int)((x - mRECT.L) / (mRECT.W() / 3.f))))); }
  bool IsDirty() override
  {
    // the play mode knob: entering arp/latch shows the arp layer, back to poly shows the delay layer
    if (auto* d = GetDelegate())
    {
      const double mo = d->GetParam(PARAM_INDEX_MS20__Voices_arpmode)->Value();
      const int zone = (mo < 18.0) ? 0 : 1;
      if (zone != mZone) { mZone = zone; SetLayer(zone ? 2 : 0); }
    }
    return IControl::IsDirty();
  }
private:
  int mLayer = 0, mZone = -1;
  std::function<void(int)> mOn;
};

// ---------------------------------------------------------------- patch panel
struct Jack { int id; const char* top; const char* bottom; int kind; int src; int param; };
// kind: 1 output, 2 input (a PatchBay parameter), 3 mono input (VCO 1+2 CV, KBD CV only), 4 tap input (ESP SIGNAL IN, SWITCH only), 0 not used yet
static const Jack kJacks[35] = {
  {0, "TOTAL", "IN", 2, 0, PARAM_INDEX_PatchBay_total},        {5, "FREQ", "IN", 2, 0, PARAM_INDEX_PatchBay_freq},
  {24, "VCO 2", "CV IN", 2, 0, PARAM_INDEX_PatchBay_vco2freq}, {25, "VCO 1+2", "CV IN", 3, 0, -1},
  {9, "EXT", "SIG IN", 0, 0, -1},
  {1, "MG", "TRI", 1, 1, -1},  {6, "MG", "SQ", 1, 2, -1},  {2, "S&H", "IN", 2, 0, PARAM_INDEX_PatchBay_shin},
  {4, "S&H", "CLOCK", 2, 0, PARAM_INDEX_PatchBay_shclock},     {7, "S&H", "OUT", 1, 7, -1},
  {18, "EG 1", "TRIG IN", 2, 0, PARAM_INDEX_PatchBay_eg1trig}, {10, "EG 1", "OUT", 1, 3, -1}, {11, "EG 1", "REV", 1, 4, -1},
  {26, "EG 2", "TRIG IN", 2, 0, PARAM_INDEX_PatchBay_eg2trig}, {22, "EG 2", "REV", 1, 6, -1},
  {13, "HPF", "CUTOFF", 2, 0, PARAM_INDEX_PatchBay_hpf},       {17, "LPF", "CUTOFF", 2, 0, PARAM_INDEX_PatchBay_lpf},
  {14, "VCA", "CTRL IN", 2, 0, PARAM_INDEX_PatchBay_vca},      {12, "VCA", "IN", 0, 0, -1}, {16, "VCA", "OUT", 0, 0, -1},
  {21, "INITIAL", "GAIN", 0, 0, -1}, {19, "NOISE", "PINK", 1, 8, -1}, {23, "NOISE", "WHITE", 1, 8, -1},
  {27, "WHEEL", "OUT", 1, 9, -1},   {33, "SWITCH", "OUT", 1, 10, -1},
  {31, "KBD", "CV OUT", 1, 11, -1}, {32, "KBD", "TRIG OUT", 1, 12, -1}, {3, "ESP", "SIG IN", 4, 0, -1},
  {8, "ESP", "AMP OUT", 0, 0, -1},  {15, "ESP", "BPF OUT", 0, 0, -1},
  {20, "ESP", "F-V OUT", 0, 0, -1}, {28, "ESP", "ENV OUT", 0, 0, -1}, {34, "ESP", "TRIG OUT", 0, 0, -1},
  {29, "SIGNAL", "OUT", 0, 0, -1},  {30, "PHONES", "", 0, 0, -1},
};

class PatchPanel : public IControl
{
public:
  static const int COLS = 5;
  PatchPanel(const IRECT& r, PatchHost* host) : IControl(r), mHost(host) {}

  IRECT Cell(int i) const
  {
    const float cw = (mRECT.W() - 16.f) / COLS, ch = (mRECT.H() - 30.f) / 7.f;
    const int c = i % COLS, row = i / COLS;
    return IRECT(mRECT.L + 8.f + c * cw, mRECT.T + 24.f + row * ch, mRECT.L + 8.f + (c + 1) * cw, mRECT.T + 24.f + (row + 1) * ch);
  }
  void JackXY(int i, float& x, float& y) const { const IRECT c = Cell(i); x = c.MW(); y = c.T + 34.f; }
  int HitJack(float x, float y) const
  {
    for (int i = 0; i < 35; i++) { float jx, jy; JackXY(i, jx, jy); if ((x - jx) * (x - jx) + (y - jy) * (y - jy) < 12.f * 12.f) return i; }
    return -1;
  }
  static int IndexOfId(int id) { for (int i = 0; i < 35; i++) if (kJacks[i].id == id) return i; return -1; }

  // the cables as they are now: input jack index -> output jack index
  struct Cable { int in, out; };
  std::vector<Cable> Cables() const
  {
    std::vector<Cable> v;
    auto* d = const_cast<PatchPanel*>(this)->GetDelegate();
    for (int i = 0; i < 35; i++)
    {
      const Jack& j = kJacks[i];
      if (j.kind == 2 && d)
      {
        const int s = (int)std::lround(d->GetParam(j.param)->Value());
        if (s >= 1 && s <= 12)
          for (int o = 0; o < 35; o++)
            if (kJacks[o].kind == 1 && kJacks[o].src == s && (s != 8 || (kJacks[o].id == 23) == (mHost->NoisePick(i) == 1))) { v.push_back({i, o}); break; }
      }
      else if (j.kind == 3 && mHost->CableFlag(0)) v.push_back({i, IndexOfId(31)});
      else if (j.kind == 4 && mHost->CableFlag(1)) v.push_back({i, IndexOfId(33)});
    }
    return v;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(kCard, mRECT, 6.f);
    g.DrawText(T(12.f, kOr, EAlign::Near), "PATCH PANEL", mRECT.GetFromTop(20.f).GetHShifted(10.f));
    g.DrawText(T(9.f, kDim, EAlign::Far), "drag between jacks  \xC2\xB7  click a plug to pull it out", mRECT.GetFromTop(20.f).GetHShifted(-10.f));
    auto* d = GetDelegate();
    for (int i = 0; i < 35; i++)
    {
      const Jack& j = kJacks[i];
      const IRECT c = Cell(i);
      const bool used = j.kind != 0;
      g.DrawText(T(9.5f, used ? kInk : kFaint), j.top, IRECT(c.L, c.T + 1.f, c.R, c.T + 12.f));
      g.DrawText(T(8.5f, used ? kDim : kFaint), j.bottom, IRECT(c.L, c.T + 12.f, c.R, c.T + 21.f));
      float x, y; JackXY(i, x, y);
      const bool hot = (mHover == i || mDragFrom == i) && used;
      g.FillCircle(used ? (j.kind == 1 ? kOrDim : kKnob) : kBg, x, y, 9.f);
      g.DrawCircle(used ? (hot ? kInk : (j.kind == 1 ? kOr : kDim)) : kFaint, x, y, 9.f, nullptr, hot ? 2.f : 1.3f);
      g.FillCircle(used ? kBg : kCard, x, y, 4.f);
      // a plug in an input with no cable (patched on the MS-20ic with the other end loose): breaks the normal
      if (j.kind == 2 && d && (int)std::lround(d->GetParam(j.param)->Value()) == 13) g.FillCircle(kInk, x, y, 5.f);
    }
    // cables: a sagging curve with a plug at each end
    static const IColor kCableCols[5] = { IColor(255, 224, 85, 43), IColor(255, 230, 190, 70), IColor(255, 80, 160, 210), IColor(255, 120, 190, 110), IColor(255, 210, 110, 170) };
    int n = 0;
    for (const Cable& cb : Cables())
    {
      if (cb.in == mLifted) continue;
      float x1, y1, x2, y2; JackXY(cb.out, x1, y1); JackXY(cb.in, x2, y2);
      DrawCable(g, x1, y1, x2, y2, kCableCols[(cb.in * 7 + cb.out) % 5]); n++;
    }
    if (mDragFrom >= 0)
    {
      float x1, y1; JackXY(mDragFrom, x1, y1);
      DrawCable(g, x1, y1, mDragX, mDragY, kInk.WithOpacity(0.85f));
    }
  }

  void DrawCable(IGraphics& g, float x1, float y1, float x2, float y2, const IColor& col)
  {
    const float dist = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    const float sag = 18.f + dist * 0.3f;
    IStrokeOptions o; o.mCapOption = ELineCap::Round;
    g.PathMoveTo(x1, y1);
    g.PathCubicBezierTo(x1, y1 + sag, x2, y2 + sag, x2, y2);
    g.PathStroke(IPattern(COLOR_BLACK.WithOpacity(0.35f)), 6.5f, o);
    g.PathMoveTo(x1, y1);
    g.PathCubicBezierTo(x1, y1 + sag, x2, y2 + sag, x2, y2);
    g.PathStroke(IPattern(col), 4.f, o);
    g.FillCircle(col, x1, y1, 6.f); g.FillCircle(col, x2, y2, 6.f);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.4f), x1, y1, 6.f); g.DrawCircle(COLOR_BLACK.WithOpacity(0.4f), x2, y2, 6.f);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const int j = HitJack(x, y);
    if (j < 0 || kJacks[j].kind == 0) return;
    mDragX = x; mDragY = y; mLifted = -1;
    // a plug in an input: pick the cable up by that end (its output end stays)
    for (const Cable& cb : Cables()) if (cb.in == j) { mLifted = j; mDragFrom = cb.out; Disconnect(j); SetDirty(false); return; }
    mDragFrom = j; SetDirty(false);
  }
  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override { if (mDragFrom >= 0) { mDragX = x; mDragY = y; mHover = HitJack(x, y); SetDirty(false); } }
  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    if (mDragFrom < 0) return;
    const int a = mDragFrom, b = HitJack(x, y);
    mDragFrom = -1; mLifted = -1;
    if (b >= 0 && b != a) Connect(a, b);   // dropped anywhere else: the cable is out
    SetDirty(false);
  }
  void OnMouseOver(float x, float y, const IMouseMod&) override { const int h = HitJack(x, y); if (h != mHover) { mHover = h; SetDirty(false); } }
  void OnMouseOut() override { mHover = -1; SetDirty(false); }
  bool IsDirty() override { if (++mTick >= 15) { mTick = 0; return true; } return IControl::IsDirty(); }   // cables patched on the MS-20ic show up

private:
  void SetInput(int param, int src)
  {
    auto* d = GetDelegate(); if (!d) return;
    const double norm = d->GetParam(param)->ToNormalized((double)src);
    d->BeginInformHostOfParamChangeFromUI(param);
    d->SendParameterValueFromUI(param, norm);
    d->EndInformHostOfParamChangeFromUI(param);
  }
  void Disconnect(int in)
  {
    const Jack& j = kJacks[in];
    if (j.kind == 2) SetInput(j.param, 0);
    else if (j.kind == 3) mHost->SetCableFlag(0, false);
    else if (j.kind == 4) mHost->SetCableFlag(1, false);
  }
  void Connect(int a, int b)
  {
    int out = a, in = b;
    if (kJacks[out].kind != 1) std::swap(out, in);
    if (kJacks[out].kind != 1 || kJacks[in].kind < 2) return;   // output to output, or a jack that does nothing yet
    const Jack& o = kJacks[out]; const Jack& i = kJacks[in];
    if (i.kind == 2) { if (o.src == 8) mHost->SetNoisePick(in, o.id == 23 ? 1 : 0); SetInput(i.param, o.src); }
    else if (i.kind == 3) { if (o.id == 31) mHost->SetCableFlag(0, true); }   // the engine's mono mode: KBD CV only
    else if (i.kind == 4) { if (o.id == 33) mHost->SetCableFlag(1, true); }   // tap tempo: SWITCH only
  }
  PatchHost* mHost;
  int mDragFrom = -1, mLifted = -1, mHover = -1, mTick = 0;
  float mDragX = 0.f, mDragY = 0.f;
};

// ---------------------------------------------------------------- memory keys
class MemoryStrip : public IControl
{
public:
  MemoryStrip(const IRECT& r, MemoryHost* host) : IControl(r), mHost(host) {}
  // layout: buttons and the name on the left, 22 white keys (3 octaves + top C) on the right
  IRECT Buttons() const { return mRECT.GetFromLeft(210.f).GetPadded(-8.f); }
  IRECT KeysR() const { return IRECT(mRECT.L + 214.f, mRECT.T + 22.f, mRECT.R - 8.f, mRECT.B - 8.f); }
  IRECT Btn(int i) const { const IRECT b = Buttons(); const float w = (b.W() - 6.f) / 3.f; return IRECT(b.L + i * (w + 3.f), b.B - 24.f, b.L + i * (w + 3.f) + w, b.B); }
  IRECT SwitchR() const { const IRECT b = Buttons(); return IRECT(b.L, b.B - 54.f, b.R, b.B - 30.f); }
  static bool IsBlack(int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
  IRECT KeyRect(int n) const   // n = 0..36
  {
    const IRECT k = KeysR(); const float ww = k.W() / 22.f;
    int whites = 0; for (int i = 0; i < n; i++) if (!IsBlack(i)) whites++;
    if (!IsBlack(n)) return IRECT(k.L + whites * ww + 1.f, k.T, k.L + (whites + 1) * ww - 1.f, k.B);
    const float x = k.L + whites * ww; return IRECT(x - ww * 0.32f, k.T, x + ww * 0.32f, k.T + k.H() * 0.6f);
  }
  int HitKey(float x, float y) const
  {
    for (int n = 0; n <= 36; n++) if (IsBlack(n) && KeyRect(n).Contains(x, y)) return n;
    for (int n = 0; n <= 36; n++) if (!IsBlack(n) && KeyRect(n).Contains(x, y)) return n;
    return -1;
  }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(kCard, mRECT, 6.f);
    g.DrawText(T(12.f, kOr, EAlign::Near), "MEMORY", mRECT.GetFromTop(20.f).GetHShifted(10.f));
    const int slot = mHost->MemSlot();
    const int show = mHover >= 0 ? mHover : (slot >= 0 ? slot : 36);
    WDL_String a, b;
    if (show == 36) { a.Set("PANEL"); b.Set("the knobs as they are"); }
    else { a.SetFormatted(16, "%d", show + 1); b.Set(mHost->MemValid(show) ? mHost->MemName(show) : "(empty)"); }
    const IRECT bt = Buttons();
    g.DrawText(T(26.f, kInk, EAlign::Near, EVAlign::Top), a.Get(), IRECT(bt.L, bt.T + 10.f, bt.R, bt.T + 40.f));
    g.DrawText(T(13.f, kInk, EAlign::Near, EVAlign::Top), b.Get(), IRECT(bt.L, bt.T + 42.f, bt.R, bt.T + 60.f));
    if (mSaveArmed) g.DrawText(T(10.5f, kOr, EAlign::Near, EVAlign::Top), "click a key to store this sound", IRECT(bt.L, bt.T + 60.f, bt.R, bt.T + 74.f));
    // momentary switch
    g.FillRoundRect(mSwitch ? kOr : kKnob, SwitchR(), 3.f);
    g.DrawText(T(10.f, mSwitch ? kBg : kDim), "SWITCH (hold)", SwitchR());
    static const char* kBtn[3] = { "SAVE", "IMPORT", "EXPORT" };
    for (int i = 0; i < 3; i++)
    {
      const bool on = (i == 0 && mSaveArmed);
      g.FillRoundRect(on ? kOr : kKnob, Btn(i), 3.f);
      g.DrawText(T(10.f, on ? kBg : kInk), kBtn[i], Btn(i));
    }
    // keys
    for (int pass = 0; pass < 2; pass++)
      for (int n = 0; n <= 36; n++)
      {
        if (IsBlack(n) != (pass == 1)) continue;
        const IRECT k = KeyRect(n);
        const bool cur = (n == slot) || (n == 36 && slot < 0);
        IColor c = IsBlack(n) ? kBlackKey : kWhiteKey;
        if (n == mHover) c = IsBlack(n) ? IColor(255, 60, 60, 64) : IColor(255, 240, 237, 230);
        g.FillRoundRect(c, k, 0.f, 0.f, 3.f, 3.f);
        if (!IsBlack(n)) g.DrawRect(IColor(255, 120, 118, 112), k, nullptr, 0.6f);
        if (cur) g.FillRect(kOr, IRECT(k.L + 2.f, k.B - 5.f, k.R - 2.f, k.B - 2.f));
        if (n < 36 && mHost->MemValid(n)) g.FillCircle(IsBlack(n) ? kDim : IColor(255, 110, 108, 104), k.MW(), k.B - 12.f, 2.f);
        if (n == 36) g.DrawText(T(8.f, IColor(255, 90, 88, 84)), "PANEL", k.GetFromBottom(26.f));
        else if (n % 12 == 0) { WDL_String s; s.SetFormatted(8, "C%d", n / 12 + 1); g.DrawText(T(8.f, IColor(255, 90, 88, 84)), s.Get(), k.GetFromBottom(26.f)); }
      }
  }
  void OnMouseDown(float x, float y, const IMouseMod& m) override
  {
    if (SwitchR().Contains(x, y)) { mSwitch = true; mHost->SwitchDown(true); SetDirty(false); return; }
    for (int i = 0; i < 3; i++) if (Btn(i).Contains(x, y)) { Button(i); return; }
    const int n = HitKey(x, y);
    if (n < 0) return;
    if (mSaveArmed && n < 36) { mHost->MemSave(n); mSaveArmed = false; mRename = n; GetUI()->CreateTextEntry(*this, T(13.f, kInk), Buttons().GetFromTop(64.f).GetFromBottom(22.f), mHost->MemName(n), 0); }
    else { mHost->MemRecall(n); mSaveArmed = false; }
    SetDirty(false);
  }
  void OnMouseUp(float, float, const IMouseMod&) override { if (mSwitch) { mSwitch = false; mHost->SwitchDown(false); SetDirty(false); } }
  void OnMouseDblClick(float x, float y, const IMouseMod&) override
  {
    const int n = HitKey(x, y);
    if (n >= 0 && n < 36) { mRename = n; GetUI()->CreateTextEntry(*this, T(13.f, kInk), Buttons().GetFromTop(64.f).GetFromBottom(22.f), mHost->MemName(n), 0); }
  }
  void OnTextEntryCompletion(const char* str, int) override { if (mRename >= 0 && str && *str) mHost->SetMemName(mRename, str); mRename = -1; SetDirty(false); }
  void OnMouseOver(float x, float y, const IMouseMod&) override { const int h = HitKey(x, y); if (h != mHover) { mHover = h; SetDirty(false); } }
  void OnMouseOut() override { mHover = -1; SetDirty(false); }
  bool IsDirty() override { if (++mTick >= 10) { mTick = 0; const int s = mHost->MemSlot(); if (s != mLastSlot) { mLastSlot = s; return true; } } return IControl::IsDirty(); }
private:
  void Button(int i)
  {
    if (i == 0) { mSaveArmed = !mSaveArmed; SetDirty(false); return; }
    WDL_String file, path;
    if (i == 1)
      GetUI()->PromptForFile(file, path, EFileAction::Open, "bin", [this](const WDL_String& f, const WDL_String&) { if (f.GetLength()) mHost->MemImport(f.Get()); SetDirty(false); });
    else
    {
      file.Set("MS20MEM.BIN");
      GetUI()->PromptForFile(file, path, EFileAction::Save, "bin", [this](const WDL_String& f, const WDL_String&) { if (f.GetLength()) mHost->MemExport(f.Get()); });
    }
  }
  MemoryHost* mHost;
  int mHover = -1, mRename = -1, mTick = 0, mLastSlot = -3;
  bool mSaveArmed = false, mSwitch = false;
};

// ---------------------------------------------------------------- header
class Header : public IControl
{
public:
  Header(const IRECT& r, std::function<int()> controller, std::function<void(char*, int)> status) : IControl(r), mCtl(controller), mStatus(status) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    g.DrawText(T(20.f, kInk, EAlign::Near), "TWENTY ENGINE", mRECT.GetHShifted(16.f));
    g.DrawText(T(20.f, kOr, EAlign::Near), "STUDIO", mRECT.GetHShifted(172.f));
    static const char* kNames[] = { "", "MS-20ic", "Ksoloti Core", "Teensy" };
    const int c = mCtl ? mCtl() : 0;
    WDL_String s; if (c > 0) s.SetFormatted(64, "%s connected", kNames[c]); else s.Set("Play from any MIDI keyboard  \xC2\xB7  an MS-20ic moves the knobs");
    const IRECT rr = mRECT.GetHShifted(-16.f);
    g.DrawText(T(11.5f, c > 0 ? kInk : kDim, EAlign::Far), s.Get(), rr.GetFromTop(mRECT.H() * 0.6f));
    IRECT tb; const float tw = g.MeasureText(T(11.5f, kInk), s.Get(), tb);
    if (c > 0) g.FillCircle(kOr, rr.R - tw - 12.f, mRECT.T + mRECT.H() * 0.3f, 4.f);
    char st[200] = ""; if (mStatus) mStatus(st, sizeof(st));
    if (*st) g.DrawText(T(10.f, kDim, EAlign::Far), st, rr.GetFromBottom(mRECT.H() * 0.4f));
  }
  bool IsDirty() override { if (++mTick >= 30) { mTick = 0; return true; } return IControl::IsDirty(); }
private:
  std::function<int()> mCtl; std::function<void(char*, int)> mStatus; int mTick = 0;
};

// ---------------------------------------------------------------- the whole window
static const int kW = 1200, kH = 690;

inline void Layout(IGraphics* g, PatchHost* ph, MemoryHost* mh, std::function<int()> controller, std::function<void(char*, int)> status)
{
  g->AttachPanelBackground(kBg);
  g->AttachControl(new Header(IRECT(0, 0, kW, 46), controller, status));
  const float ch = 92.f, gap = 10.f, x0 = 16.f, rowW = 830.f;   // every row ends where the patch panel begins
  float cw = 62.f;
  auto rowCells = [&](int sections, int knobs) { cw = (rowW - sections * 12.f - (sections - 1) * gap) / (float)knobs; };
  float x = x0, y = 52.f;
  auto section = [&](const char* title, int n) { IRECT r(x, y, x + n * cw + 12.f, y + ch + 24.f); g->AttachControl(new Section(r, title)); const float sx = x + 6.f; x += n * cw + 12.f + gap; return sx; };
  auto cell = [&](float sx, int i) { return IRECT(sx + i * cw, y + 20.f, sx + (i + 1) * cw, y + 20.f + ch); };
  const std::vector<std::string> scale = { "32'", "16'", "8'", "4'" };
  // row 1: oscillators, mixer, filters
  rowCells(5, 12);
  float s = section("VCO 1", 3);
  g->AttachControl(new Selector(cell(s, 0), PARAM_INDEX_MS20__Voices_vco1oct, "Scale", scale));
  g->AttachControl(new Selector(cell(s, 1), PARAM_INDEX_MS20__Voices_vco1wave, "Wave", { "Sine", "Saw", "Pulse", "Noise" }));
  g->AttachControl(new Knob(cell(s, 2), PARAM_INDEX_MS20__Voices_pw, "PW", "shapes all"));
  s = section("VCO 2", 3);
  g->AttachControl(new Selector(cell(s, 0), PARAM_INDEX_MS20__Voices_vco2oct, "Scale", scale));
  g->AttachControl(new Selector(cell(s, 1), PARAM_INDEX_MS20__Voices_vco2wave, "Wave", { "Tri", "Saw", "Square", "Ring" }));
  g->AttachControl(new Knob(cell(s, 2), PARAM_INDEX_MS20__Voices_vco2semi, "Pitch"));
  s = section("MIXER", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_Master__Mixer_vco1, "VCO 1"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_Master__Mixer_vco2, "VCO 2"));
  s = section("HPF", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_Filter__Macro_hpfcut, "Cutoff"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_Filter__Macro_hpfpeak, "Peak"));
  s = section("LPF", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_Filter__Macro_lpfcut, "Cutoff"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_Filter__Macro_lpfpeak, "Peak"));
  // row 2: pitch, modulation amounts, MG
  x = x0; y += ch + 24.f + 8.f; rowCells(5, 10);
  s = section("PITCH", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_MS20__Voices_portamento, "Portamento", "Multibend in poly"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_MS20__Master__Tune_tune, "Tune"));
  s = section("FREQ MOD", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_MG__Morphing__LFO_pitchmod, "MG", "MG/T.EXT"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_EG1_amount, "EG 1", "EG1/EXT"));
  s = section("HPF MOD", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_MG__Morphing__LFO_hpfmod, "MG", "MG/T.EXT"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_EG2_hpfdepth, "EG 2", "EG2/EXT"));
  s = section("LPF MOD", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_MG__Morphing__LFO_lpfmod, "MG", "MG/T.EXT"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_EG2_lpfdepth, "EG 2", "EG2/EXT"));
  s = section("MG", 2);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_MG__Morphing__LFO_shape, "Wave", "", { "TRI", "SQ", "RND" }));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_MG__Morphing__LFO_rate, "Freq"));
  // row 3: envelopes, output
  x = x0; y += ch + 24.f + 8.f; rowCells(3, 12);
  s = section("EG 1", 3);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_EG1_delay, "Delay"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_EG1_attack, "Attack"));
  g->AttachControl(new Knob(cell(s, 2), PARAM_INDEX_EG1_release, "Release"));
  s = section("EG 2", 5);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_EG2_hold, "Hold", "full = latch"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_EG2_attack, "Attack"));
  g->AttachControl(new Knob(cell(s, 2), PARAM_INDEX_EG2_decay, "Decay"));
  g->AttachControl(new Knob(cell(s, 3), PARAM_INDEX_EG2_sustain, "Sustain"));
  g->AttachControl(new Knob(cell(s, 4), PARAM_INDEX_EG2_release, "Release"));
  s = section("OUTPUT", 4);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_VCA__Macro_drive, "Drive"));
  g->AttachControl(new Knob(cell(s, 1), PARAM_INDEX_VCA__Macro_bassboost, "Bass"));
  g->AttachControl(new Knob(cell(s, 2), PARAM_INDEX_Filter__Macro_clicknoise, "Key Click"));
  g->AttachControl(new Knob(cell(s, 3), PARAM_INDEX_VCA__Macro_mastervol, "Volume"));
  // row 4: the ESP knobs (play mode + four knobs on three layers)
  x = x0; y += ch + 24.f + 8.f; cw = 76.f;
  s = section("ESP  \xC2\xB7  PLAY MODE, DELAY, ARP", 5);
  g->AttachControl(new Knob(cell(s, 0), PARAM_INDEX_MS20__Voices_arpmode, "Play Mode", "SIGNAL LEVEL", { "POLY", "ARP", "LATCH" }));
  Knob* esp[4];
  for (int i = 0; i < 4; i++) esp[i] = new Knob(cell(s, i + 1), kEspLayers[0].params[i], kEspLayers[0].labels[i], kEspHardware[i]);
  for (int i = 0; i < 4; i++) g->AttachControl(esp[i]);
  auto relayer = [esp, g](int l) {
    for (int i = 0; i < 4; i++)
    {
      const int p = kEspLayers[l].params[i];
      esp[i]->SetParamIdx(p);
      esp[i]->SetValue(g->GetDelegate()->GetParam(p)->GetNormalized());
      esp[i]->SetLabels(kEspLayers[l].labels[i], kEspHardware[i]);
    }
  };
  // the layer switch, in a card of its own next to the knobs
  const float swl = x, swr = x0 + rowW;
  g->AttachControl(new Section(IRECT(swl, y, swr, y + ch + 24.f), "ESP KNOBS"));
  g->AttachControl(new EspSwitch(IRECT(swl + 12.f, y + 34.f, swr - 12.f, y + 58.f), relayer));
  g->AttachControl(new ITextControl(IRECT(swl + 12.f, y + 66.f, swr - 12.f, y + 80.f), "Picks what the four ESP knobs do.", T(10.f, kDim, EAlign::Near), COLOR_TRANSPARENT));
  g->AttachControl(new ITextControl(IRECT(swl + 12.f, y + 81.f, swr - 12.f, y + 95.f), "Follows the play mode; click to change.", T(10.f, kDim, EAlign::Near), COLOR_TRANSPARENT));
  // the patch panel
  g->AttachControl(new PatchPanel(IRECT(862.f, 52.f, kW - 16.f, 52.f + 4 * (ch + 24.f) + 3 * 8.f), ph));
  // memory keys
  g->AttachControl(new MemoryStrip(IRECT(16.f, kH - 136.f, kW - 16.f, kH - 12.f), mh));
  g->AttachCornerResizer(EUIResizerMode::Scale, false);
}

} // namespace te_ui
