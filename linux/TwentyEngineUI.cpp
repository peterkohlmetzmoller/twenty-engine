// Twenty Engine for Linux: the panel window. The same panel code as the Mac plugin window (ui/StudioUI.h), run on
// the small IGraphics stand-in in linux/shim/IControls.h. The window scales with its size (aspect kept).
#include "DistrhoUI.hpp"
#include "TwentyEnginePlugin.hpp"
#include "IControls.h"
#include "ui/StudioUI.h"
#include "resources/font_cond.h"

START_NAMESPACE_DISTRHO

using namespace iplug;
using namespace iplug::igraphics;

class TwentyEngineUI : public UI, public IEditorDelegate
{
public:
  TwentyEngineUI()
  : UI(te_ui::kW, te_ui::kH)
  {
    mPlug = static_cast<TwentyEnginePlugin*>(getPluginInstancePointer());
    createFontFromMemory("Cond", (const uchar*)kCondTTF, (uint)sizeof(kCondTTF), false);
    createFontFromMemory("CondB", (const uchar*)kCondBTTF, (uint)sizeof(kCondBTTF), false);
    createFontFromMemory("CondI", (const uchar*)kCondITTF, (uint)sizeof(kCondITTF), false);
    for (int i = 0; i < kNumParams; i++)
    {
      int type; double def, mn, mx; TwentyEnginePlugin::ParamRange(i, type, def, mn, mx);
      mParams[i].Init(type == 0 ? IParam::kTypeDouble : (type == 1 ? IParam::kTypeInt : IParam::kTypeBool), def, mn, mx);
      mShown[i] = mPlug->EngineRaw(i);
      mParams[i].Set(TwentyEnginePlugin::FromRaw(i, mShown[i]));
    }
    mG = std::make_unique<IGraphics>(this);
    te_ui::Layout(mG.get(), mPlug, [this]() { return mPlug->Controller(); }, [this](char* out, int n) { Diag(out, n); });
    setGeometryConstraints(te_ui::kW / 2, te_ui::kH / 2, true, false);
  }

  // ---- IEditorDelegate
  IParam* GetParam(int idx) override { return (idx >= 0 && idx < kNumParams) ? &mParams[idx] : nullptr; }
  void SendParameterValueFromUI(int idx, double normalized) override
  {
    if (idx < 0 || idx >= kNumParams) return;
    mParams[idx].SetNormalized(normalized);
    const double v = mParams[idx].Value();
    mShown[idx] = TwentyEnginePlugin::ToRaw(idx, v);
    setParameterValue((uint32_t)idx, (float)v);
  }
  void BeginInformHostOfParamChangeFromUI(int idx) override { if (idx >= 0) editParameter((uint32_t)idx, true); }
  void EndInformHostOfParamChangeFromUI(int idx) override { if (idx >= 0) editParameter((uint32_t)idx, false); }

protected:
  // the knobs follow the engine (the MS-20ic, a memory recalled on the keys), except one turned in the window just now
  void uiIdle() override
  {
    const int64_t now = TwentyEnginePlugin::NowMs();
    for (int i = 0; i < kNumParams; i++)
    {
      const int32_t v = mPlug->EngineRaw(i);
      if (v == mShown[i] || now - mPlug->TouchedMs(i) < 1500) continue;
      mShown[i] = v;
      mParams[i].Set(TwentyEnginePlugin::FromRaw(i, v));
    }
    if (mG->Tick()) repaint();
  }
  void parameterChanged(uint32_t, float) override {}

  void onNanoDisplay() override
  {
    const float s = Scale();
    save();
    beginPath(); rect(0, 0, getWidth(), getHeight()); fillColor(Color(255, 255, 255)); fill();
    translate(OffX(), OffY());
    scale(s, s);
    mG->DrawAll(*this);
    restore();
  }
  bool onMouse(const MouseEvent& ev) override
  {
    if (ev.button != 1) return false;
    const float x = ToX(ev.pos.getX()), y = ToY(ev.pos.getY());
    if (ev.press) mG->MouseDown(x, y, Mod(ev.mod, true), ev.time); else mG->MouseUp(x, y, Mod(ev.mod, false));
    repaint();
    return true;
  }
  bool onMotion(const MotionEvent& ev) override
  {
    mG->MouseMove(ToX(ev.pos.getX()), ToY(ev.pos.getY()), Mod(ev.mod, false));
    if (mG->Tick()) repaint();
    return true;
  }
  bool onScroll(const ScrollEvent& ev) override
  {
    mG->Wheel(ToX(ev.pos.getX()), ToY(ev.pos.getY()), Mod(ev.mod, false), (float)ev.delta.getY());
    repaint();
    return true;
  }

private:
  // drawn at the panel's own size, scaled to fit the window (centred)
  float Scale() const { return std::min(getWidth() / (float)te_ui::kW, getHeight() / (float)te_ui::kH); }
  float OffX() const { return (getWidth() - te_ui::kW * Scale()) * 0.5f; }
  float OffY() const { return (getHeight() - te_ui::kH * Scale()) * 0.5f; }
  float ToX(double x) const { return (float)((x - OffX()) / Scale()); }
  float ToY(double y) const { return (float)((y - OffY()) / Scale()); }
  static IMouseMod Mod(uint m, bool left) { return IMouseMod(left, false, (m & kModifierShift) != 0, (m & kModifierControl) != 0, (m & kModifierAlt) != 0); }
  void Diag(char* out, int n) const
  {
    const int ps = mPlug->PanelSource();
    if (mPlug->Controller() == 0) snprintf(out, n, "Plays from any MIDI keyboard  \xC2\xB7  plug in the MS-20ic for knobs and LEDs");
    else snprintf(out, n, "%s", ps == 2 ? "Panel read from the MS-20ic" : ps == 1 ? "Panel from memory  \xC2\xB7  volume off/on reads the knobs" : "Switch the volume off and on to read the knobs");
  }

  TwentyEnginePlugin* mPlug = nullptr;
  IParam mParams[ms20::NPARAMS];
  int32_t mShown[ms20::NPARAMS] = {};
  std::unique_ptr<IGraphics> mG;

  DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TwentyEngineUI)
};

UI* createUI() { return new TwentyEngineUI(); }

END_NAMESPACE_DISTRHO
