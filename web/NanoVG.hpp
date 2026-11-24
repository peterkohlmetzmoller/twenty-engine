// NanoVG.hpp (web) - the few DGL::NanoVG calls the panel's IGraphics stand-in (linux/shim/IControls.h) makes,
// on plain NanoVG (WebGL). The Linux app gets the same calls from DPF.
#pragma once
#include "nanovg.h"

namespace DGL {

struct Color
{
  float r = 0, g = 0, b = 0, a = 1;
  Color() {}
  Color(int ri, int gi, int bi, float al = 1.f) : r(ri / 255.f), g(gi / 255.f), b(bi / 255.f), a(al) {}
  NVGcolor nvg() const { return nvgRGBAf(r, g, b, a); }
};

template <typename T> struct Rectangle
{
  T x = 0, y = 0, w = 0, h = 0;
  T getWidth() const { return w; }
  T getHeight() const { return h; }
};

class NanoVG
{
public:
  enum LineCap { BUTT = NVG_BUTT, ROUND = NVG_ROUND, SQUARE = NVG_SQUARE, MITER = NVG_MITER, BEVEL = NVG_BEVEL };
  enum Winding { CCW = NVG_CCW, CW = NVG_CW };
  enum Align { ALIGN_LEFT = NVG_ALIGN_LEFT, ALIGN_CENTER = NVG_ALIGN_CENTER, ALIGN_RIGHT = NVG_ALIGN_RIGHT,
               ALIGN_TOP = NVG_ALIGN_TOP, ALIGN_MIDDLE = NVG_ALIGN_MIDDLE, ALIGN_BOTTOM = NVG_ALIGN_BOTTOM, ALIGN_BASELINE = NVG_ALIGN_BASELINE };
  explicit NanoVG(NVGcontext* c) : mC(c) {}
  NVGcontext* context() const { return mC; }
  void beginPath() { nvgBeginPath(mC); }
  void closePath() { nvgClosePath(mC); }
  void moveTo(float x, float y) { nvgMoveTo(mC, x, y); }
  void lineTo(float x, float y) { nvgLineTo(mC, x, y); }
  void bezierTo(float a, float b, float c, float d, float x, float y) { nvgBezierTo(mC, a, b, c, d, x, y); }
  void rect(float x, float y, float w, float h) { nvgRect(mC, x, y, w, h); }
  void circle(float x, float y, float r) { nvgCircle(mC, x, y, r); }
  void arc(float cx, float cy, float r, float a0, float a1, int dir) { nvgArc(mC, cx, cy, r, a0, a1, dir); }
  void fillColor(const Color& c) { nvgFillColor(mC, c.nvg()); }
  void strokeColor(const Color& c) { nvgStrokeColor(mC, c.nvg()); }
  void strokeWidth(float w) { nvgStrokeWidth(mC, w); }
  void lineCap(int c) { nvgLineCap(mC, c); }
  void lineJoin(int j) { nvgLineJoin(mC, j); }
  void fill() { nvgFill(mC); }
  void stroke() { nvgStroke(mC); }
  void fontFace(const char* f) { nvgFontFace(mC, f); }
  void fontSize(float s) { nvgFontSize(mC, s); }
  void textAlign(int a) { nvgTextAlign(mC, a); }
  float text(float x, float y, const char* s, const char* end) { return nvgText(mC, x, y, s, end); }
  float textBounds(float x, float y, const char* s, const char* end, Rectangle<float>& r)
  {
    float b[4] = {0, 0, 0, 0}; const float adv = nvgTextBounds(mC, x, y, s, end, b);
    r.x = b[0]; r.y = b[1]; r.w = b[2] - b[0]; r.h = b[3] - b[1];
    return adv;
  }
private:
  NVGcontext* mC;
};

} // namespace DGL
