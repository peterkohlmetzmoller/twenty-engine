// ui.cpp - Twenty Engine in the browser, the panel: the same panel code as every edition (ui/StudioUI.h) on the
// small IGraphics stand-in of the Linux app (linux/shim/IControls.h), drawn by NanoVG with WebGL.
// The page (app.js) passes in the mouse, the engine's values, the LEDs, MIDI and the controller's state; the panel
// sends parameter changes and MIDI back to the page, which hands them to the sound (worklet.js + dsp.wasm).
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include "nanovg.h"
#define NANOVG_GLES2_IMPLEMENTATION
#include "nanovg_gl.h"
#include "../engine/engine_gen.h"
#include "../engine/cc_map.h"
#include "NanoVG.hpp"
#include "IControls.h"
#include "../ui/StudioUI.h"
#include "../resources/font_cond.h"

using namespace iplug;
using namespace iplug::igraphics;
using namespace ms20;

// ---- to the page
EM_JS(void, js_set, (int i, int raw), { if (Module.onSet) Module.onSet(i, raw); });
EM_JS(void, js_midi, (int s, int d1, int d2), { if (Module.onMidi) Module.onMidi(s, d1, d2); });
EM_JS(void, js_copy_log, (), { if (Module.onCopyLog) Module.onCopyLog(); });
EM_JS(void, js_mail_log, (), { if (Module.onMailLog) Module.onMailLog(); });
EM_JS(void, js_cursor, (int hand), { if (Module.onCursor) Module.onCursor(hand); });
EM_JS(void, js_save_ccmap, (const char* s), { if (Module.onSaveCcMap) Module.onSaveCcMap(UTF8ToString(s)); });
EM_JS(void, js_log, (const char* s), { if (Module.onLog) Module.onLog(UTF8ToString(s)); });

static int64_t NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static bool IsFrac(int i) { return strncmp(kParamInfo[i].type, "frac32", 6) == 0; }
static bool IsBipolar(int i) { return strncmp(kParamInfo[i].type, "frac32.s", 8) == 0; }
static bool IsBool(int i) { return strncmp(kParamInfo[i].type, "bool32", 6) == 0; }
static int32_t ToRaw(int i, double v)
{
  if (IsFrac(i)) { double r = v * 2097152.0; if (r > 134217727.0) r = 134217727.0; if (r < -134217728.0) r = -134217728.0; return (int32_t)lrint(r); }
  return (int32_t)lrint(v);
}
static double FromRaw(int i, int32_t raw) { return IsFrac(i) ? raw / 2097152.0 : (double)raw; }

// the panel's host in the page: the same jobs as the plugin's (TwentyEngine.cpp) and the Linux app's
class WebHost : public te_ui::PatchHost, public IEditorDelegate
{
public:
  WebHost()
  {
    for (int i = 0; i < NPARAMS; i++)
    {
      const ParamInfo& p = kParamInfo[i];
      if (IsFrac(i)) mParams[i].Init(IParam::kTypeDouble, p.def / 2097152.0, IsBipolar(i) ? -64.0 : 0.0, 64.0);
      else if (IsBool(i)) mParams[i].Init(IParam::kTypeBool, p.def != 0, 0.0, 1.0);
      else mParams[i].Init(IParam::kTypeInt, p.def, 0.0, (p.hmax > 0) ? p.hmax - 1 : 13);
      mShown[i] = ToRaw(i, mParams[i].Value());
    }
  }
  // ---- IEditorDelegate: a knob turned in the panel -> the sound
  IParam* GetParam(int i) override { return (i >= 0 && i < NPARAMS) ? &mParams[i] : nullptr; }
  void SendParameterValueFromUI(int i, double n) override
  {
    if (i < 0 || i >= NPARAMS) return;
    mParams[i].SetNormalized(n);
    mShown[i] = ToRaw(i, mParams[i].Value()); mTouch[i] = NowMs();
    js_set(i, mShown[i]);
  }
  void BeginInformHostOfParamChangeFromUI(int) override {}
  void EndInformHostOfParamChangeFromUI(int) override {}
  // ---- the page -> the panel
  void Engine(int i, int raw)   // the engine's value (the MS-20ic's knob, a memory): the knob follows unless just turned here
  {
    if (i < 0 || i >= NPARAMS) return;
    if (i == PARAM_INDEX_Patch__State_mono) mMono = raw;
    if (raw == mShown[i] || NowMs() - mTouch[i] < 1500) return;
    mShown[i] = raw; mParams[i].Set(FromRaw(i, raw));
  }
  // MIDI from the page: CC learn first (a keyboard's controller, not the iC's), then the sound; the tap cable for the panel
  void Midi(int s, int d1, int d2, bool fromIc)
  {
    if ((s & 0xF0) == 0xB0 && !fromIc)
    {
      // the iC's own messages never come here (fromIc): any controller on another keyboard can be learned, even
      // one that has the same number as an iC knob (a plugin can't tell them apart, the page can)
      const int p = GlobalCcMap().onCc(d1, false);
      if (p >= 0 && p < NPARAMS)
      {
        const double v = mParams[p].FromNormalized(d2 / 127.0);
        mParams[p].Set(v); mShown[p] = ToRaw(p, v); mTouch[p] = NowMs();
        js_set(p, mShown[p]);
        return;
      }
    }
    if (!fromIc && (s & 0xF0) == 0xD0) return;   // a keyboard's aftertouch is never the iC's switch
    if (fromIc) WatchCable(s, d1, d2);
    js_midi(s, d1, d2);
  }
  void Poll()   // every frame: the CC map after a change goes to the page's storage
  {
    const int ch = GlobalCcMap().changed.load();
    if (ch != mCcSaved)
    {
      mCcSaved = ch;
      GlobalCcMap().save("/ccmap.txt");
      if (FILE* f = fopen("/ccmap.txt", "r")) { std::string t; char b[256]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) t.append(b, n); fclose(f); js_save_ccmap(t.c_str()); }
    }
  }
  void LoadCcMap(const char* text)
  {
    if (FILE* f = fopen("/ccmap.txt", "w")) { fputs(text, f); fclose(f); }
    GlobalCcMap().load("/ccmap.txt"); mCcSaved = GlobalCcMap().changed.load();
  }

  // ---- te_ui::PatchHost (the panel asks)
  bool CableFlag(int w) const override { return (w & 1) ? mTap : mMono != 0; }
  void SetCableFlag(int w, bool on) override
  {
    w &= 1; const int a = w ? 33 : 31, b = w ? 3 : 25;
    if (w) mTap = on;
    if (on) Cable(4, a, b); else { Cable(5, a, b); Cable(5, b, b); Cable(5, a, a); }
  }
  int NoisePick(int j) const override { return (j >= 0 && j < 35) ? mNoise[j] : 0; }
  void SetNoisePick(int j, int pick) override { if (j >= 0 && j < 35) mNoise[j] = (uint8_t)pick; }
  void SwitchDown(bool down) override { js_midi(0xD0, down ? 127 : 0, 0); }
  int Leds() const override { return mLeds; }
  void CopyLog() override { js_copy_log(); }
  bool LogSavedToFile() const override { return false; }
  void MailLog() override { js_mail_log(); }
  void PatchCable(int o, int i, bool on) override { if (on) Cable(4, o, i); else { Cable(5, o, i); Cable(5, i, i); Cable(5, o, o); } }
  void SetEspLatch(bool on) override { js_midi(0xB0, 105, on ? 127 : 0); }
  bool CcAvailable() const override { return true; }
  int CcOf(int p) const override { return GlobalCcMap().ccOf(p); }
  int CcLearning() const override { return GlobalCcMap().learning.load(); }
  void CcLearn(int p) override { GlobalCcMap().learning.store(p); }
  void CcForget(int p) override { GlobalCcMap().forget(p); }
  int CcTakeLearned() override { return GlobalCcMap().learnedCc.exchange(-1); }

  int mController = 0, mLeds = 0x10, mMidiState = 0;
  std::unique_ptr<IGraphics> mG;
private:
  void Cable(int t, int a, int b) { js_midi(0xB0, 99, t); js_midi(0xB0, 98, a); js_midi(0xB0, 6, b); }
  void WatchCable(int s, int d1, int d2)   // the iC's tap cable (SWITCH -> ESP SIGNAL IN), for the panel
  {
    if ((s & 0xF0) != 0xB0) return;
    if (d1 == 99) { mCw99 = d2; mH99 = true; mH98 = false; return; }
    if (d1 == 98) { mCw98 = d2; mH98 = true; return; }
    if (d1 != 6 || !mH99 || !mH98) return;
    mH99 = mH98 = false;
    const int t = mCw99, a = mCw98, b = d2;
    if ((a == 33 && b == 3) || (a == 3 && b == 33)) mTap = t == 4;
    if (t == 5 && a == b && (a == 33 || a == 3)) mTap = false;
  }
  IParam mParams[NPARAMS];
  int32_t mShown[NPARAMS] = {};
  int64_t mTouch[NPARAMS] = {};
  uint8_t mNoise[35] = {};
  int mMono = 0; bool mTap = false; int mCcSaved = 0;
  int mCw99 = -1, mCw98 = -1; bool mH99 = false, mH98 = false;
};

static std::unique_ptr<WebHost> gHost;
static NVGcontext* gVG = nullptr;
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gGL = 0;
static float gScale = 1.f, gOffX = 0.f, gOffY = 0.f;
static int gW = 0, gH = 0;

static void Diag(char* out, int n)
{
  switch (gHost ? gHost->mMidiState : 0)
  {
    case 0: snprintf(out, n, "Free web edition  \xC2\xB7  needs Chrome or Edge with MIDI allowed"); break;
    case 1: snprintf(out, n, "Plug in the MS-20ic (USB) or any MIDI keyboard"); break;
    case 2: snprintf(out, n, "MS-20ic found  \xC2\xB7  LEDs need SysEx (allow it when asked)"); break;
    default: snprintf(out, n, "Switch the iC's volume off and on to read its knobs"); break;
  }
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int te_ui_init()
{
  EmscriptenWebGLContextAttributes a; emscripten_webgl_init_context_attributes(&a);
  a.stencil = 1; a.antialias = 1; a.alpha = 0; a.premultipliedAlpha = 0; a.majorVersion = 1;
  gGL = emscripten_webgl_create_context("#panel", &a);
  if (gGL <= 0) return 0;
  emscripten_webgl_make_context_current(gGL);
  gVG = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  if (!gVG) return 0;
  nvgCreateFontMem(gVG, "Cond", (unsigned char*)kCondTTF, (int)sizeof(kCondTTF), 0);
  nvgCreateFontMem(gVG, "CondB", (unsigned char*)kCondBTTF, (int)sizeof(kCondBTTF), 0);
  nvgCreateFontMem(gVG, "CondI", (unsigned char*)kCondITTF, (int)sizeof(kCondITTF), 0);
  gHost = std::make_unique<WebHost>();
  gHost->mG = std::make_unique<IGraphics>(gHost.get());
  gHost->mG->mCursorFn = [](ECursor c) { js_cursor(c == ECursor::HAND ? 1 : 0); };
  te_ui::Layout(gHost->mG.get(), gHost.get(), []() { return gHost->mController; }, [](char* out, int n) { Diag(out, n); });
  return 1;
}

// a frame: draws when something changed (or the canvas was resized); w/h in CSS pixels, dpr = device pixels per CSS pixel
EMSCRIPTEN_KEEPALIVE int te_ui_frame(int w, int h, float dpr, int force)
{
  if (!gHost) return 0;
  gHost->Poll();
  const bool resized = w != gW || h != gH;
  if (!gHost->mG->Tick() && !resized && !force) return 0;
  gW = w; gH = h;
  gScale = std::min(w / (float)te_ui::kW, h / (float)te_ui::kH);
  gOffX = (w - te_ui::kW * gScale) * 0.5f; gOffY = (h - te_ui::kH * gScale) * 0.5f;
  emscripten_webgl_make_context_current(gGL);
  glViewport(0, 0, (int)(w * dpr), (int)(h * dpr));
  glClearColor(1.f, 1.f, 1.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  nvgBeginFrame(gVG, (float)w, (float)h, dpr);
  nvgTranslate(gVG, gOffX, gOffY); nvgScale(gVG, gScale, gScale);
  DGL::NanoVG vg(gVG);
  gHost->mG->DrawAll(vg);
  nvgEndFrame(gVG);
  return 1;
}

// mouse: type 0 down, 1 up, 2 move, 3 wheel (delta), 4 left the canvas; x/y in CSS pixels; button 0 left, 2 right
EMSCRIPTEN_KEEPALIVE void te_ui_mouse(int type, float x, float y, int button, int mods, double timeMs, float delta)
{
  if (!gHost) return;
  const float px = (x - gOffX) / gScale, py = (y - gOffY) / gScale;
  const bool shift = mods & 1, ctrl = mods & 2, alt = mods & 4;
  IGraphics& g = *gHost->mG;
  if (type == 0) g.MouseDown(px, py, IMouseMod(button != 2, button == 2, shift, ctrl, alt), (uint32_t)timeMs);
  else if (type == 1) g.MouseUp(px, py, IMouseMod(false, false, shift, ctrl, alt));
  else if (type == 2) g.MouseMove(px, py, IMouseMod(false, false, shift, ctrl, alt));
  else if (type == 3) g.Wheel(px, py, IMouseMod(false, false, shift, ctrl, alt), delta);
  else g.MouseLeave();
}

EMSCRIPTEN_KEEPALIVE void te_ui_engine(int i, int raw) { if (gHost) gHost->Engine(i, raw); }
EMSCRIPTEN_KEEPALIVE void te_ui_leds(int bits) { if (gHost) gHost->mLeds = bits; }
// the controller: 0 none, 1 MS-20ic; MIDI: 0 no access, 1 access (no iC), 2 iC without SysEx (no LEDs), 3 iC with LEDs
EMSCRIPTEN_KEEPALIVE void te_ui_status(int controller, int midiState) { if (gHost) { gHost->mController = controller; gHost->mMidiState = midiState; } }
EMSCRIPTEN_KEEPALIVE void te_ui_midi(int s, int d1, int d2, int fromIc) { if (gHost) gHost->Midi(s, d1, d2, fromIc != 0); }
EMSCRIPTEN_KEEPALIVE void te_ui_ccmap(const char* text) { if (gHost) gHost->LoadCcMap(text); }
EMSCRIPTEN_KEEPALIVE int te_ui_kw() { return te_ui::kW; }
EMSCRIPTEN_KEEPALIVE int te_ui_kh() { return te_ui::kH; }

}
