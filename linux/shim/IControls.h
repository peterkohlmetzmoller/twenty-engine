// IControls.h (Linux) - the small part of iPlug2's IGraphics that ui/StudioUI.h uses, drawn with DPF's NanoVG.
// iPlug2 has no Linux windows yet, so the Linux app keeps the panel code as it is and runs it on this instead.
// Same names, same behaviour where the panel depends on it: colours are A,R,G,B; arcs in degrees clockwise from
// 12 o'clock; knobs turn by vertical drag (iPlug's gearing), double-click resets, the wheel steps 1 %.
#pragma once
#include "NanoVG.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace iplug {

static const int kNoParameter = -1;

// a parameter as the panel sees it: value in its own units, normalised 0..1
class IParam
{
public:
  enum EType { kTypeDouble, kTypeInt, kTypeBool };
  void Init(EType t, double def, double mn, double mx) { mType = t; mDefault = def; mMin = mn; mMax = mx; mValue = def; }
  double Value() const { return mValue; }
  void Set(double v) { mValue = Constrain(v); }
  double GetNormalized() const { return ToNormalized(mValue); }
  double ToNormalized(double v) const { return mMax > mMin ? std::max(0.0, std::min(1.0, (v - mMin) / (mMax - mMin))) : 0.0; }
  double FromNormalized(double n) const { return Constrain(mMin + std::max(0.0, std::min(1.0, n)) * (mMax - mMin)); }
  void SetNormalized(double n) { mValue = FromNormalized(n); }
  double GetDefault(bool normalized = false) const { return normalized ? ToNormalized(mDefault) : mDefault; }
  bool GetStepped() const { return mType != kTypeDouble; }
  double GetStep() const { return GetStepped() ? 1.0 : 0.01; }
  double ConstrainNormalized(double n) const { return ToNormalized(FromNormalized(n)); }
  EType Type() const { return mType; }
private:
  double Constrain(double v) const { v = std::max(mMin, std::min(mMax, v)); return mType == kTypeDouble ? v : std::round(v); }
  EType mType = kTypeDouble; double mDefault = 0, mMin = 0, mMax = 1, mValue = 0;
};

// what the window talks to (the app)
class IEditorDelegate
{
public:
  virtual ~IEditorDelegate() {}
  virtual IParam* GetParam(int idx) = 0;
  virtual void SendParameterValueFromUI(int idx, double normalized) = 0;
  virtual void BeginInformHostOfParamChangeFromUI(int idx) = 0;
  virtual void EndInformHostOfParamChangeFromUI(int idx) = 0;
};

namespace igraphics {

struct IColor
{
  int A = 255, R = 0, G = 0, B = 0;
  IColor() {}
  IColor(int a, int r, int g, int b) : A(a), R(r), G(g), B(b) {}
  DGL::Color Dgl() const { return DGL::Color(R, G, B, A / 255.f); }
};

struct IRECT
{
  float L = 0, T = 0, R = 0, B = 0;
  IRECT() {}
  IRECT(float l, float t, float r, float b) : L(l), T(t), R(r), B(b) {}
  float W() const { return R - L; }
  float H() const { return B - T; }
  float MW() const { return (L + R) * 0.5f; }
  float MH() const { return (T + B) * 0.5f; }
  bool Contains(float x, float y) const { return x >= L && x < R && y >= T && y < B; }
  IRECT GetPadded(float p) const { return IRECT(L - p, T - p, R + p, B + p); }
  IRECT GetFromBottom(float h) const { return IRECT(L, B - h, R, B); }
  IRECT GetHShifted(float x) const { return IRECT(L + x, T, R + x, B); }
};

enum class EAlign { Near, Center, Far };
enum class EVAlign { Top, Middle, Bottom };
enum class EDirection { Vertical, Horizontal };
enum class ELineCap { Butt, Round, Square };
enum class ELineJoin { Miter, Round, Bevel };
enum class EUIResizerMode { Scale, Size };
struct IBlend {};

struct IText
{
  float mSize = 14.f; IColor mFGColor; std::string mFont; EAlign mAlign = EAlign::Center; EVAlign mVAlign = EVAlign::Middle;
  IText(float size = 14.f, const IColor& c = IColor(), const char* font = "Cond", EAlign a = EAlign::Center, EVAlign v = EVAlign::Middle)
  : mSize(size), mFGColor(c), mFont(font ? font : "Cond"), mAlign(a), mVAlign(v) {}
};
struct IPattern { IColor mColor; IPattern(const IColor& c) : mColor(c) {} };
struct IStrokeOptions { ELineCap mCapOption = ELineCap::Butt; ELineJoin mJoinOption = ELineJoin::Miter; };
struct IFillOptions {};
struct IMouseMod
{
  bool L = false, R = false, S = false, C = false, A = false;
  IMouseMod() {}
  IMouseMod(bool l, bool r, bool s, bool c, bool a) : L(l), R(r), S(s), C(c), A(a) {}
};

class IGraphics;

class IControl
{
public:
  IControl(const IRECT& r, int paramIdx = kNoParameter) : mRECT(r), mParamIdx(paramIdx) {}
  virtual ~IControl() {}
  virtual void Draw(IGraphics& g) = 0;
  virtual bool IsDirty() { return mDirty; }
  virtual bool IsHit(float x, float y) const { return mRECT.Contains(x, y); }
  virtual void OnMouseDown(float, float, const IMouseMod&) {}
  virtual void OnMouseUp(float, float, const IMouseMod&) {}
  virtual void OnMouseDrag(float, float, float, float, const IMouseMod&) {}
  virtual void OnMouseDblClick(float, float, const IMouseMod&)
  {
    if (const IParam* p = GetParam()) { SetValue(p->GetDefault(true)); SetDirty(true); }
  }
  virtual void OnMouseOver(float, float, const IMouseMod&) {}
  virtual void OnMouseOut() {}
  virtual void OnMouseWheel(float, float, const IMouseMod&, float) {}

  // value: normalised, the control's own copy (kept in step with the parameter by the window)
  double GetValue() const { return mValue; }
  void SetValue(double v, int = 0) { mValue = std::max(0.0, std::min(1.0, v)); }
  int GetParamIdx(int = 0) const { return mParamIdx; }
  void SetParamIdx(int i) { mParamIdx = i; }
  const IParam* GetParam(int = 0) const { return (mParamIdx >= 0 && mDelegate) ? mDelegate->GetParam(mParamIdx) : nullptr; }
  IEditorDelegate* GetDelegate() const { return mDelegate; }
  IGraphics* GetUI() const { return mGraphics; }
  // repaint; with triggerAction the parameter takes the control's value (a knob turned in the window)
  void SetDirty(bool triggerAction = true, int = 0)
  {
    mDirty = true;
    if (triggerAction && mParamIdx >= 0 && mDelegate) mDelegate->SendParameterValueFromUI(mParamIdx, mValue);
  }
  void SetClean() { mDirty = false; }
  bool MouseIsDown() const { return mMouseIsDown; }

  IRECT mRECT;
  bool mIgnoreMouse = false;
protected:
  int mParamIdx = kNoParameter;
  double mValue = 0.0;
  bool mDirty = true;
private:
  friend class IGraphics;
  IEditorDelegate* mDelegate = nullptr;
  IGraphics* mGraphics = nullptr;
  bool mMouseIsDown = false;
};

class IKnobControlBase : public IControl
{
public:
  IKnobControlBase(const IRECT& r, int paramIdx = kNoParameter, EDirection dir = EDirection::Vertical, double gearing = 4.0)
  : IControl(r, paramIdx), mDirection(dir), mGearing(gearing) {}
  void OnMouseDown(float, float, const IMouseMod&) override { mMouseDown = true; mMouseDragValue = GetValue(); }
  void OnMouseUp(float, float, const IMouseMod&) override { mMouseDown = false; SetDirty(false); }
  void OnMouseDrag(float, float, float dX, float dY, const IMouseMod& mod) override
  {
    const double gearing = (mod.C || mod.S) ? mGearing * 10.0 : mGearing;
    if (mDirection == EDirection::Vertical) mMouseDragValue += (double)dY / (double)(mRECT.T - mRECT.B) / gearing;
    else mMouseDragValue += (double)dX / (double)(mRECT.R - mRECT.L) / gearing;
    mMouseDragValue = std::max(0.0, std::min(1.0, mMouseDragValue));
    double v = mMouseDragValue;
    if (const IParam* p = GetParam()) if (p->GetStepped()) v = p->ConstrainNormalized(v);
    SetValue(v);
    SetDirty();
  }
  void OnMouseWheel(float, float, const IMouseMod& mod, float d) override
  {
    const IParam* p = GetParam();
    if (p && p->GetStepped()) { const double n = p->Value() + (d > 0 ? 1.0 : -1.0); SetValue(p->ToNormalized(n)); }
    else SetValue(GetValue() + d * ((mod.C || mod.S) ? 0.001 : 0.01));
    SetDirty();
  }
  void OnMouseOver(float, float, const IMouseMod&) override {}
  void OnMouseOut() override {}
protected:
  EDirection mDirection; double mGearing; double mMouseDragValue = 0.0; bool mMouseDown = false;
};

// the window: controls, drawing (NanoVG), mouse
class IGraphics
{
public:
  explicit IGraphics(IEditorDelegate* d) : mDelegate(d) {}
  IEditorDelegate* GetDelegate() const { return mDelegate; }
  IControl* AttachControl(IControl* c)
  {
    c->mDelegate = mDelegate; c->mGraphics = this;
    if (const IParam* p = c->GetParam()) c->SetValue(p->GetNormalized());
    mControls.emplace_back(c);
    return c;
  }
  void AttachCornerResizer(EUIResizerMode = EUIResizerMode::Scale, bool = false) {}   // the app's window resizes itself

  // ---- the app drives these ----
  // once per frame: controls follow their parameters (unless held), then ask each whether it needs drawing
  bool Tick()
  {
    bool dirty = false;
    for (auto& c : mControls)
    {
      if (const IParam* p = c->GetParam())
        if (!c->mMouseIsDown && std::fabs(p->GetNormalized() - c->GetValue()) > 1e-9) { c->SetValue(p->GetNormalized()); c->SetDirty(false); }
      if (c->IsDirty()) dirty = true;
    }
    return dirty;
  }
  void DrawAll(DGL::NanoVG& vg)
  {
    mVG = &vg;
    for (auto& c : mControls) { c->Draw(*this); c->SetClean(); }
    mVG = nullptr;
  }
  void MouseDown(float x, float y, const IMouseMod& m, uint32_t timeMs)
  {
    IControl* c = Hit(x, y);
    const bool dbl = c && c == mLastClick && timeMs - mLastClickMs < 350;
    mLastClick = c; mLastClickMs = timeMs;
    if (!c) return;
    mCapture = c; mLastX = x; mLastY = y;
    if (dbl) { const int p = c->GetParamIdx(); if (p >= 0) mDelegate->BeginInformHostOfParamChangeFromUI(p); c->OnMouseDblClick(x, y, m); if (p >= 0) mDelegate->EndInformHostOfParamChangeFromUI(p); mCapture = nullptr; mLastClick = nullptr; return; }
    c->mMouseIsDown = true;
    mCaptureParam = c->GetParamIdx();
    if (mCaptureParam >= 0) mDelegate->BeginInformHostOfParamChangeFromUI(mCaptureParam);
    c->OnMouseDown(x, y, m);
  }
  void MouseUp(float x, float y, const IMouseMod& m)
  {
    if (!mCapture) return;
    IControl* c = mCapture; mCapture = nullptr;
    c->OnMouseUp(x, y, m);
    c->mMouseIsDown = false;
    if (mCaptureParam >= 0) mDelegate->EndInformHostOfParamChangeFromUI(mCaptureParam);
    mCaptureParam = kNoParameter;
    MouseMove(x, y, m);
  }
  void MouseMove(float x, float y, const IMouseMod& m)
  {
    if (mCapture) { mCapture->OnMouseDrag(x, y, x - mLastX, y - mLastY, m); mLastX = x; mLastY = y; return; }
    IControl* c = Hit(x, y);
    if (c != mOver) { if (mOver) mOver->OnMouseOut(); mOver = c; }
    if (c) c->OnMouseOver(x, y, m);
  }
  void MouseLeave() { if (mOver && !mCapture) { mOver->OnMouseOut(); mOver = nullptr; } }
  void Wheel(float x, float y, const IMouseMod& m, float d)
  {
    IControl* c = Hit(x, y); if (!c) return;
    const int p = c->GetParamIdx();
    if (p >= 0) mDelegate->BeginInformHostOfParamChangeFromUI(p);
    c->OnMouseWheel(x, y, m, d);
    if (p >= 0) mDelegate->EndInformHostOfParamChangeFromUI(p);
  }

  // ---- drawing (iPlug's calls) ----
  void FillRect(const IColor& c, const IRECT& r, const IBlend* = nullptr) { V().beginPath(); V().rect(r.L, r.T, r.W(), r.H()); V().fillColor(c.Dgl()); V().fill(); }
  void DrawRect(const IColor& c, const IRECT& r, const IBlend* = nullptr, float t = 1.f) { V().beginPath(); V().rect(r.L, r.T, r.W(), r.H()); Stroke(c, t); }
  void DrawLine(const IColor& c, float x1, float y1, float x2, float y2, const IBlend* = nullptr, float t = 1.f)
  { V().beginPath(); V().moveTo(x1, y1); V().lineTo(x2, y2); V().lineCap(DGL::NanoVG::BUTT); Stroke(c, t); }
  void DrawCircle(const IColor& c, float cx, float cy, float r, const IBlend* = nullptr, float t = 1.f) { V().beginPath(); V().circle(cx, cy, r); Stroke(c, t); }
  void FillCircle(const IColor& c, float cx, float cy, float r, const IBlend* = nullptr) { V().beginPath(); V().circle(cx, cy, r); V().fillColor(c.Dgl()); V().fill(); }
  // degrees clockwise from 12 o'clock (iPlug) -> radians from 3 o'clock (NanoVG)
  void DrawArc(const IColor& c, float cx, float cy, float r, float a1, float a2, const IBlend* = nullptr, float t = 1.f)
  {
    const float k = 3.14159265f / 180.f;
    V().beginPath(); V().arc(cx, cy, r, (a1 - 90.f) * k, (a2 - 90.f) * k, DGL::NanoVG::CW); Stroke(c, t);
  }
  void DrawText(const IText& t, const char* s, const IRECT& r, const IBlend* = nullptr)
  {
    if (!s || !*s) return;
    V().fontFace(t.mFont.c_str()); V().fontSize(t.mSize);
    const int h = t.mAlign == EAlign::Near ? DGL::NanoVG::ALIGN_LEFT : (t.mAlign == EAlign::Far ? DGL::NanoVG::ALIGN_RIGHT : DGL::NanoVG::ALIGN_CENTER);
    const int v = t.mVAlign == EVAlign::Top ? DGL::NanoVG::ALIGN_TOP : (t.mVAlign == EVAlign::Bottom ? DGL::NanoVG::ALIGN_BOTTOM : DGL::NanoVG::ALIGN_MIDDLE);
    const float x = t.mAlign == EAlign::Near ? r.L : (t.mAlign == EAlign::Far ? r.R : r.MW());
    const float y = t.mVAlign == EVAlign::Top ? r.T : (t.mVAlign == EVAlign::Bottom ? r.B : r.MH());
    V().textAlign(h | v); V().fillColor(t.mFGColor.Dgl());
    V().text(x, y, s, nullptr);
  }
  void PathMoveTo(float x, float y) { if (!mPath) { V().beginPath(); mPath = true; } V().moveTo(x, y); }
  void PathLineTo(float x, float y) { V().lineTo(x, y); }
  void PathCubicBezierTo(float c1x, float c1y, float c2x, float c2y, float x, float y) { V().bezierTo(c1x, c1y, c2x, c2y, x, y); }
  void PathClose() { V().closePath(); }
  void PathStroke(const IPattern& p, float t, const IStrokeOptions& o = IStrokeOptions())
  {
    V().lineCap(o.mCapOption == ELineCap::Round ? DGL::NanoVG::ROUND : (o.mCapOption == ELineCap::Square ? DGL::NanoVG::SQUARE : DGL::NanoVG::BUTT));
    V().lineJoin(o.mJoinOption == ELineJoin::Round ? DGL::NanoVG::ROUND : (o.mJoinOption == ELineJoin::Bevel ? DGL::NanoVG::BEVEL : DGL::NanoVG::MITER));
    Stroke(p.mColor, t); mPath = false;
  }
  void PathFill(const IPattern& p, const IFillOptions& = IFillOptions()) { V().fillColor(p.mColor.Dgl()); V().fill(); mPath = false; }

private:
  DGL::NanoVG& V() { return *mVG; }
  void Stroke(const IColor& c, float t) { V().strokeColor(c.Dgl()); V().strokeWidth(t); V().stroke(); V().lineCap(DGL::NanoVG::BUTT); V().lineJoin(DGL::NanoVG::MITER); }
  IControl* Hit(float x, float y)
  {
    for (auto it = mControls.rbegin(); it != mControls.rend(); ++it)
      if (!(*it)->mIgnoreMouse && (*it)->mRECT.Contains(x, y) && (*it)->IsHit(x, y)) return it->get();
    return nullptr;
  }
  IEditorDelegate* mDelegate;
  std::vector<std::unique_ptr<IControl>> mControls;
  DGL::NanoVG* mVG = nullptr;
  bool mPath = false;
  IControl *mCapture = nullptr, *mOver = nullptr, *mLastClick = nullptr;
  int mCaptureParam = kNoParameter;
  uint32_t mLastClickMs = 0;
  float mLastX = 0, mLastY = 0;
};

} // namespace igraphics
} // namespace iplug
