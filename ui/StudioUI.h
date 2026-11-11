// StudioUI.h - the panel window of Twenty Engine UI.
// The MS-20 panel layout (the same coordinates as manual/panel.py, traced from the MS-20ic), drawn in the look of
// the Twenty Engine pictures: white panel, black line art, orange outputs, blue inputs, green cable gestures, grey for
// what the engine does not use. Below it a status bar with the memory, as in the window mock-up.
//  - Knobs and switches are the plugin's parameters: the DAW records and automates them; an MS-20ic moves them.
//  - The ESP knobs carry three layers (DELAY / TONE / ARP); a switch in the ESP section picks the layer, and each
//    knob's current job is printed above it in orange (as on the ESP sticker). The MS-20 name stays below.
//  - Patch cables: drag between jacks; grab a plug at an input to move or pull out the cable. Inputs are the PatchBay
//    parameters (saved, automated, mirrored from the MS-20ic). VCO 1+2 CV IN (mono) and ESP SIGNAL IN (tap tempo) are
//    cable states, sent to the engine as the MS-20ic's own cable messages.
#pragma once
#include "IControls.h"
#include "StudioHost.h"
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace te_ui {
using namespace iplug;
using namespace iplug::igraphics;
using namespace ms20;

// ---------------------------------------------------------------- look (the Twenty Engine pictures)
// black and white only; grey only for what is greyed out (jacks the engine does not use)
static const IColor kPaper(255, 255, 255, 255), kWin = kPaper, kInk(255, 0, 0, 0), kGrey(255, 170, 170, 170),
  kDimC = kGrey, kAccent = kInk, kOutC = kInk, kInC = kInk, kMonoC = kInk, kLed = kInk, kBar = kPaper;
static const char *kF = "Cond", *kFB = "CondB", *kFI = "CondI";

// panel geometry: units of manual/panel.py (1 unit = 1 mm on the drawing), scaled to pixels
static const float PW = 200.f, PH = 86.f, LW = 100.f, S = 6.f, OX = 12.f, OY = 12.f;
static const float BARH = 42.f;
static const int kW = (int)(OX * 2 + PW * S), kH = (int)(OY + PH * S + 10.f + BARH);
static float X(float u) { return OX + u * S; }                 // panel x
static float Yd(float yd) { return OY + yd * S; }              // measured down from the top
static float Yu(float y) { return OY + (PH - y) * S; }         // measured up from the bottom
static const float RSX = (PW - LW - 2.f) / 1300.f, RSY = PH / 1060.f;
static float RXu(float px) { return LW + 1.f + (px - 190.f) * RSX; }   // patch panel photo grid -> units
static float RYu(float py) { return (1140.f - py) * RSY; }
static float RX(float px) { return X(RXu(px)); }
static float RY(float py) { return Yu(RYu(py)); }
static float Rad(float d) { return d * 3.14159265f / 180.f; }

// text like panel.py: (x, baseline) in pixels, size in units, lines downwards
static void Txt(IGraphics& g, float x, float base, const char* s, float size, const IColor& c = kInk, char anchor = 'c', const char* font = kF)
{
  const float px = size * S;
  std::string all(s); size_t start = 0; int line = 0;
  while (start <= all.size())
  {
    size_t nl = all.find('\n', start); if (nl == std::string::npos) nl = all.size();
    const std::string ln = all.substr(start, nl - start);
    const float b = base + line * px * 1.08f, cy = b - px * 0.36f;
    const EAlign a = anchor == 'l' ? EAlign::Near : (anchor == 'r' ? EAlign::Far : EAlign::Center);
    const IRECT r = anchor == 'l' ? IRECT(x, cy - px, x + 400.f, cy + px) : (anchor == 'r' ? IRECT(x - 400.f, cy - px, x, cy + px) : IRECT(x - 200.f, cy - px, x + 200.f, cy + px));
    g.DrawText(IText(px, c, font, a, EVAlign::Middle), ln.c_str(), r);
    start = nl + 1; line++;
  }
}
static int Lines(const char* s) { int n = 1; for (; *s; s++) if (*s == '\n') n++; return n; }

static void Line(IGraphics& g, float x1, float y1, float x2, float y2, float wUnits, const IColor& c = kInk) { g.DrawLine(c, x1, y1, x2, y2, nullptr, wUnits * S); }

// waveform symbols (panel.py wave()): centre (x, y) in pixels, size s in pixels
static void Wave(IGraphics& g, const char* kind, float x, float y, float s, const IColor& c = kInk, float lw = 1.3f)
{
  const float h = s * 0.5f, w = s;
  IStrokeOptions o; o.mJoinOption = ELineJoin::Round;
  const std::string k(kind);
  if (k == "ring")
  {
    g.DrawCircle(c, x, y, h * 0.55f, nullptr, lw);
    g.DrawLine(c, x - h * 0.39f, y + h * 0.39f, x + h * 0.39f, y - h * 0.39f, nullptr, lw);
    g.DrawLine(c, x - h * 0.39f, y - h * 0.39f, x + h * 0.39f, y + h * 0.39f, nullptr, lw);
    return;
  }
  std::vector<std::pair<float, float>> p;   // y up
  if (k == "tri") p = {{-w / 2, -h / 2}, {-w / 4, h / 2}, {0, -h / 2}, {w / 4, h / 2}, {w / 2, -h / 2}};
  else if (k == "saw") p = {{-w / 2, -h / 2}, {0, h / 2}, {0, -h / 2}, {w / 2, h / 2}, {w / 2, -h / 2}};
  else if (k == "sq" || k == "pulse" || k == "npulse")
  {
    const float d = k == "sq" ? 0.5f : (k == "pulse" ? 0.35f : 0.15f), x0 = -w / 2;
    p = {{x0, -h / 2}, {x0, h / 2}, {x0 + w / 2 * d * 2, h / 2}, {x0 + w / 2 * d * 2, -h / 2}, {x0 + w / 2, -h / 2}, {x0 + w / 2, h / 2},
         {x0 + w / 2 + w / 2 * d * 2, h / 2}, {x0 + w / 2 + w / 2 * d * 2, -h / 2}, {w / 2, -h / 2}};
  }
  else if (k == "noise") { const float r[13] = {0, .3f, -.4f, .5f, -.2f, .45f, -.5f, .1f, .4f, -.35f, .25f, -.45f, 0}; for (int i = 0; i <= 12; i++) p.push_back({-w / 2 + w * i / 12.f, r[i] * h * 1.2f}); }
  else if (k == "phsaw")
  {
    for (int i = 0; i <= 12; i++) { const float t = i / 12.f; p.push_back({-w / 2 + w / 2 * t, -h / 2 + h * t + ((i % 12) ? 0.28f * h / 2 * std::sin(6.2831853f * 3 * t) : 0.f)}); }
    p.push_back({0, -h / 2});
    for (int i = 0; i <= 12; i++) { const float t = i / 12.f; p.push_back({w / 2 * t, -h / 2 + h * t + ((i % 12) ? 0.28f * h / 2 * std::sin(6.2831853f * 3 * t) : 0.f)}); }
  }
  else if (k == "pink") { const float r[9] = {0, .45f, .7f, .5f, -.2f, -.6f, -.4f, .2f, 0}; for (int i = 0; i < 9; i++) p.push_back({-w / 2 + w * i / 8.f, r[i] * h / 2}); }
  else if (k == "sine") { for (int i = 0; i <= 24; i++) { const float t = i / 24.f; p.push_back({-w / 2 + w * t, std::sin(t * 6.2831853f) * h / 2}); } }
  else if (k == "rnd") { const float r[9] = {0, .7f, .2f, .9f, -.6f, -.2f, .5f, -.8f, .1f}; for (int i = 0; i < 9; i++) p.push_back({-w / 2 + w * i / 8.f, r[i] * h / 2}); }
  if (p.empty()) return;
  g.PathMoveTo(x + p[0].first, y - p[0].second);
  for (size_t i = 1; i < p.size(); i++) g.PathLineTo(x + p[i].first, y - p[i].second);
  g.PathStroke(IPattern(c), lw, o);
}

// the PW sticker's legend (label/pw_label.py morph()): one wave from knob-left to knob-right, the Twenty Engine part
// fading to orange. kind: 0 sine -> triangle, 1 saw -> phaser, 2 square -> narrow pulse, 3 white -> pink noise
static IColor Mix(const IColor& a, const IColor& b, float t)
{
  return IColor(255, (int)(a.R + (b.R - a.R) * t), (int)(a.G + (b.G - a.G) * t), (int)(a.B + (b.B - a.B) * t));
}
static void Morph(IGraphics& g, int kind, float x, float y, float w, float h, int cycles = 3)
{
  const float m = y + h / 2, A = h / 2;
  struct P { float x, y, t; };
  std::vector<P> pts;
  if (kind == 2)   // the pulse narrows cycle by cycle (all original MS-20)
  {
    for (int k = 0; k < cycles; k++)
    {
      const float t0 = (float)k / cycles, t1 = (float)(k + 1) / cycles, d = 0.5f - 0.36f * (k + 0.5f) / cycles, xe = t0 + (t1 - t0) * d;
      pts.push_back({x + w * t0, m + A, t0}); pts.push_back({x + w * t0, m - A, t0}); pts.push_back({x + w * xe, m - A, xe}); pts.push_back({x + w * xe, m + A, xe});
    }
    pts.push_back({x + w, m + A, 1.f});
  }
  else if (kind == 3)   // white -> pink: steps get slower and smoother
  {
    uint32_t rs = 21u; auto rnd = [&]() { rs = rs * 1664525u + 1013904223u; return ((rs >> 8) & 0xFFFF) / 32767.5f - 1.f; };
    float t = 0.f, prev = 0.f; pts.push_back({x, m - A * rnd(), 0.f});
    while (t < 1.f)
    {
      t = std::min(1.f, t + 0.011f + 0.05f * t * t);
      const float a = 0.55f * t; prev = a * prev + (1 - a) * rnd();
      const float v = std::max(-1.f, std::min(1.f, prev / std::max(0.45f, 1 - 0.55f * a)));
      pts.push_back({x + w * t, m - A * v, t});
    }
  }
  else
  {
    const int N = 160;
    for (int i = 0; i <= N; i++)
    {
      const float t = (float)i / N, ph = std::fmod(cycles * t, 1.f);
      float v;
      if (kind == 0) { const float tri = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph; v = (1 - t) * std::sin(6.2831853f * (ph - 0.25f)) + t * tri; }
      else v = (2 * ph - 1) * (1 - 0.25f * t) + 0.25f * t * std::sin(6.2831853f * 4 * ph);
      pts.push_back({x + w * t, m - A * v, t});
    }
  }
  for (size_t i = 0; i + 1 < pts.size(); i++)
  {
    const float t = pts[i].t;
    const IColor c = kInk; (void)t;
    g.DrawLine(c, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, nullptr, 1.4f);
  }
}

static void Arrow(IGraphics& g, std::vector<std::pair<float, float>> pts, const IColor& c = kInk)
{
  for (size_t i = 1; i < pts.size(); i++) g.DrawLine(c, pts[i - 1].first, pts[i - 1].second, pts[i].first, pts[i].second, nullptr, 1.5f);
  const float xa = pts[pts.size() - 2].first, ya = pts[pts.size() - 2].second, xb = pts.back().first, yb = pts.back().second;
  const float a = std::atan2(yb - ya, xb - xa), L = 0.9f * S;
  g.PathMoveTo(xb, yb);
  g.PathLineTo(xb - L * std::cos(a - 0.45f), yb - L * std::sin(a - 0.45f));
  g.PathLineTo(xb - L * std::cos(a + 0.45f), yb - L * std::sin(a + 0.45f));
  g.PathClose(); g.PathFill(IPattern(c));
}

static void Box(IGraphics& g, float x0, float yTop, float x1, float yBot, const char* label, float size, const IColor& c = kInk)
{
  g.DrawRect(c, IRECT(x0, yTop, x1, yBot), nullptr, 1.5f);
  const int n = Lines(label);
  Txt(g, (x0 + x1) / 2, (yTop + yBot) / 2 - (n - 1) * size * S * 0.54f + size * S * 0.35f, label, size, c);
}

// the hovered or turned control, shown in the status bar
struct Info { std::string text; int age = 0; };
static Info& HoverInfo() { static Info i; return i; }
static void SetInfo(const std::string& s) { HoverInfo().text = s; HoverInfo().age = 0; }

// ---------------------------------------------------------------- the static panel print
// the five LEDs, numbered as the engine drives them on the MS-20ic: 0 MG rate, 1 EG 1, 2 ESP PEAK (arp tempo),
// 3 ESP TRIG (patch memory), 4 power (volume)
static float LedX(int i)
{
  static const float COLX5 = 69.8f, COLX6 = 82.2f;
  switch (i) { case 0: return X(COLX5 - 1.5f); case 1: return X(COLX6 + 3.2f); case 2: return RX(1120); case 3: return RX(1385); default: return RX(1210); }
}
static float LedY(int i)
{
  switch (i) { case 0: return Yd(47.0f); case 1: return Yd(36.6f); case 2: case 3: return RY(800); default: return RY(105); }
}
class LedLights : public IControl
{
public:
  LedLights(PatchHost* h) : IControl(IRECT(0, 0, (float)kW, (float)kH)), mHost(h) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    const int m = mHost ? mHost->Leds() : 0x10;
    for (int i = 0; i < 5; i++)
    {
      const bool on = (m >> i) & 1;
      g.FillCircle(on ? kLed : kPaper, LedX(i), LedY(i), 0.7f * S);
      g.DrawCircle(kInk, LedX(i), LedY(i), 0.7f * S, nullptr, 1.5f);
    }
    mShown = m;
  }
  bool IsDirty() override { return (mHost ? mHost->Leds() : 0x10) != mShown || IControl::IsDirty(); }
  bool IsHit(float, float) const override { return false; }
private:
  PatchHost* mHost; int mShown = -1;
};

class PanelArt : public IControl
{
public:
  PanelArt() : IControl(IRECT(0, 0, (float)kW, (float)kH)) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    g.FillRect(kWin, mRECT);
    const IRECT panel(X(0), Yd(0), X(PW), Yd(PH));
    g.FillRect(kPaper, panel);
    static const float COLX[8] = {7.5f, 20.0f, 32.6f, 45.0f, 57.4f, 69.8f, 82.2f, 94.0f};
    auto mid = [&](int a, int b) { return (COLX[a] + COLX[b]) / 2; };
    const float xs[9] = {0.f, mid(0, 1), mid(1, 2), mid(2, 3), mid(3, 4), mid(4, 5), mid(5, 6), mid(6, 7), LW};
    auto sec = [&](float x0, float yd0, float x1, float yd1, const char* title, float size = 2.2f) {
      g.DrawRect(kInk, IRECT(X(x0), Yd(yd0), X(x1), Yd(yd1)), nullptr, 0.55f * S);
      Txt(g, X(x0 + 0.9f), Yd(yd0) + (size + 0.5f) * S, title, size, kInk, 'l');
    };
    sec(xs[0], 0, xs[1], 61.5f, "VCO 1"); sec(xs[1], 0, xs[2], 61.5f, "VCO 2");
    sec(xs[0], 61.5f, xs[1], PH, "PORTAMENTO", 1.6f); sec(xs[1], 61.5f, xs[2], PH, "MASTER TUNE", 1.6f);
    sec(xs[2], 0, xs[3], 45.5f, "VCO MIXER", 1.95f); sec(xs[3], 0, xs[4], 45.5f, "HPF"); sec(xs[4], 0, xs[5], 45.5f, "LPF");
    sec(xs[2], 45.5f, xs[3], PH, "FREQ MOD", 1.95f); sec(xs[3], 45.5f, xs[4], PH, "HPF MOD", 1.95f); sec(xs[4], 45.5f, xs[5], PH, "LPF MOD", 1.95f);
    sec(xs[5], 0, xs[7], 33.5f, "VCA"); sec(xs[5], 33.5f, xs[6], PH, "MG"); sec(xs[6], 33.5f, xs[7], PH, "EG 1");
    sec(xs[7], 0, xs[8], PH, "EG 2");
    g.DrawRect(kInk, IRECT(X(LW), Yd(0), X(PW), Yu(RYu(770))), nullptr, 0.55f * S);
    g.DrawRect(kInk, IRECT(X(LW), Yu(RYu(770)), X(PW), Yd(PH)), nullptr, 0.55f * S);
    g.DrawRect(kInk, panel, nullptr, 0.8f * S);
    // VCA symbol
    const float cx = (xs[5] + xs[7]) / 2, cy = 17.f;
    g.PathMoveTo(X(cx - 5), Yd(cy - 3.8f)); g.PathLineTo(X(cx + 4.5f), Yd(cy)); g.PathLineTo(X(cx - 5), Yd(cy + 3.8f)); g.PathClose();
    g.PathStroke(IPattern(kInk), 0.4f * S);
    Line(g, X(cx - 11), Yd(cy), X(cx - 5), Yd(cy), 0.4f); Line(g, X(cx + 4.5f), Yd(cy), X(cx + 10), Yd(cy), 0.4f);
    Line(g, X(cx - 0.3f), Yd(cy + 1.9f), X(cx - 0.3f), Yd(26.5f), 0.4f);
    Txt(g, X(cx - 0.3f), Yd(29.5f), "EG2 & EXT", 1.6f);
    // patch panel: signal flow
    const char* blocks[6][3] = {{"195", "320", "VCO 1"}, {"360", "485", "VCO 2"}, {"530", "600", "MIX"}, {"640", "765", "HPF"}, {"800", "920", "LPF"}, {"950", "1072", "VCA"}};
    for (auto& b : blocks) Box(g, RX((float)atof(b[0])), RY(185), RX((float)atof(b[1])), RY(115), b[2], 1.9f);
    Arrow(g, {{RX(600), RY(150)}, {RX(640), RY(150)}}); Arrow(g, {{RX(765), RY(150)}, {RX(800), RY(150)}}); Arrow(g, {{RX(920), RY(150)}, {RX(950), RY(150)}});
    Arrow(g, {{RX(485), RY(160)}, {RX(530), RY(160)}});
    Arrow(g, {{RX(258), RY(115)}, {RX(258), RY(95)}, {RX(565), RY(95)}, {RX(565), RY(115)}});
    Arrow(g, {{RX(1072), RY(150)}, {RX(1130), RY(150)}});
    Box(g, RX(300), RY(495), RX(430), RY(435), "MG", 1.9f);
    Box(g, RX(715), RY(500), RX(835), RY(440), "EG 1", 1.9f);
    Box(g, RX(880), RY(500), RX(1000), RY(440), "EG 2", 1.9f);
    Box(g, RX(310), RY(740), RX(435), RY(680), "S&H", 1.9f);
    Box(g, RX(970), RY(740), RX(1085), RY(680), "NOISE", 1.9f);
    g.PathMoveTo(RX(670), RY(735)); g.PathLineTo(RX(735), RY(710)); g.PathLineTo(RX(670), RY(685)); g.PathClose(); g.PathStroke(IPattern(kInk), 1.5f);
    Txt(g, RX(692), RY(716), "VCA", 1.3f, kInk);
    Arrow(g, {{RX(265), RY(710)}, {RX(310), RY(710)}}); Arrow(g, {{RX(435), RY(710)}, {RX(468), RY(710)}});
    Arrow(g, {{RX(365), RY(615)}, {RX(365), RY(680)}});
    Arrow(g, {{RX(630), RY(710)}, {RX(670), RY(710)}}); Arrow(g, {{RX(735), RY(710)}, {RX(775), RY(710)}});
    Arrow(g, {{RX(725), RY(612)}, {RX(725), RY(692)}});
    // ESP
    // ESP block diagram: drawn 30 grid steps higher than on the MS-20ic photo, so the knobs' arp names have room
    // (in grey: Twenty Engine uses the ESP's knobs for the delay and the arp, and SIGNAL IN, not the processor)
    Txt(g, RX(200), RY(795), "EXTERNAL SIGNAL PROCESSOR", 2.0f, kInk, 'l');
    Box(g, RX(330), RY(895), RX(410), RY(845), "AMP", 1.6f, kGrey);
    Box(g, RX(555), RY(900), RX(680), RY(840), "BAND PASS\nFILTER", 1.55f, kGrey);
    Box(g, RX(790), RY(900), RX(910), RY(840), "F-V\nCONVERTER", 1.55f, kGrey);
    Box(g, RX(1060), RY(900), RX(1180), RY(840), "ENVELOPE\nFOLLOWER", 1.55f, kGrey);
    const float ar[6][2] = {{272, 330}, {410, 468}, {512, 555}, {680, 708}, {752, 790}, {910, 943}};
    for (auto& a : ar) Arrow(g, {{RX(a[0]), RY(870)}, {RX(a[1]), RY(870)}}, kGrey);
    Arrow(g, {{RX(1180), RY(870)}, {RX(1240), RY(870)}}, kGrey);
    Arrow(g, {{RX(1300), RY(185)}, {RX(1370), RY(185)}});
    // volume print
    Txt(g, RX(1210), RY(280) - 7.4f * S, "VOLUME", 2.0f);   // above the scale marks (they reach 6.3 units up)
    Txt(g, RX(1210), RY(280) + 7.3f * S + 0.6f * S, "POWER OFF", 1.6f);
    // LED labels (the lights themselves: LedLights, driven by the engine)
    Txt(g, LedX(0), LedY(0) - (0.7f + 0.9f) * S, "RATE", 1.7f);
    Txt(g, LedX(2) - 1.6f * S, LedY(2) + 0.7f * S, "PEAK", 1.9f, kInk, 'r');
    Txt(g, LedX(3) - 1.6f * S, LedY(3) + 0.7f * S, "TRIG", 1.9f, kInk, 'r');
  }
};

// ---------------------------------------------------------------- knob (panel.py knob(): rays, white cap, pointer)
class PKnob : public IKnobControlBase
{
public:
  PKnob(float cx, float cy, float rUnits, int param, const char* label, const char* infoName, bool labelAbove = false)
  : IKnobControlBase(IRECT(cx - 6.2f * S, cy - rUnits * 2.4f * S - 2.5f * S, cx + 6.2f * S, cy + rUnits * 1.62f * S + 6.5f * S), param, EDirection::Vertical, 2.0),
    mCX(cx), mCY(cy), mR(rUnits * S), mLabel(label), mInfo(infoName), mAbove(labelAbove) {}
  void SetFunction(const char* fn, const char* info) { mFn = fn; mInfo = info; SetDirty(false); }
  void SetZones(std::vector<std::string> z) { mZones = z; }
  // label, hover name and the symbols at both ends of the scale, worked out while drawing (PW follows VCO 1's waveform)
  struct Look { std::string label, info; int morph; };   // morph: the PW sticker's wave (see Morph())
  void SetLook(std::function<Look()> f) { mLook = f; }
  void Draw(IGraphics& g) override
  {
    for (int i = 0; i < 11; i++)
    {
      const float a = Rad(240.f - 30.f * i);
      g.DrawLine(kInk, mCX + mR * 1.12f * std::cos(a), mCY - mR * 1.12f * std::sin(a), mCX + mR * 1.5f * std::cos(a), mCY - mR * 1.5f * std::sin(a), nullptr, 0.3f * S);
    }
    g.FillCircle(kPaper, mCX, mCY, mR);
    g.DrawCircle(kInk, mCX, mCY, mR, nullptr, 0.38f * S);
    const float v = (float)GetValue(), a = Rad(240.f - 300.f * std::max(0.f, std::min(1.f, v)));
    IStrokeOptions o; o.mCapOption = ELineCap::Round;
    g.PathMoveTo(mCX + mR * 0.1f * std::cos(a), mCY - mR * 0.1f * std::sin(a));
    g.PathLineTo(mCX + mR * 0.86f * std::cos(a), mCY - mR * 0.86f * std::sin(a));
    g.PathStroke(IPattern(kInk), mR * 0.13f, o);
    if (mLook)   // the PW knob: the sticker's morph wave for the selected waveform, and what it is called
    {
      const Look lk = mLook(); mInfo = lk.info;
      // the wave sits between the two lowest scale marks (there is no mark straight below), the name right under it
      const float top = mCY + mR * 1.12f, ww = 9.f * S, hh = 1.5f * S;
      Morph(g, lk.morph, mCX - ww / 2, top, ww, hh);
      Txt(g, mCX, top + hh + 1.7f * S, lk.label.c_str(), 1.72f);
    }
    else if (!mLabel.empty() && !mAbove) Txt(g, mCX, mCY + (mR / S * 1.62f + 1.5f) * S + 0.4f * S, mLabel.c_str(), 1.72f);
    // the knob's current job (ESP layers), as on the ESP sticker: orange italic above
    if (!mFn.empty()) Txt(g, mCX, mCY - mR * 1.62f - 0.3f * S, mFn.c_str(), 1.72f, kAccent);
    for (size_t i = 0; i < mZones.size(); i++)   // play mode zones around SIGNAL LEVEL
    {
      const float za = Rad(240.f - 300.f * (float)i / (float)(mZones.size() - 1));
      Txt(g, mCX + mR * 2.0f * std::cos(za), mCY - mR * 1.75f * std::sin(za) + 0.5f * S, mZones[i].c_str(), 1.3f, kAccent);
    }
  }
  void OnMouseOver(float x, float y, const IMouseMod& m) override { IKnobControlBase::OnMouseOver(x, y, m); Report(); SetDirty(false); }
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& m) override { IKnobControlBase::OnMouseDrag(x, y, dX, dY, m); Report(); }
  void OnMouseOut() override { IKnobControlBase::OnMouseOut(); SetDirty(false); }
  bool IsHit(float x, float y) const override { return (x - mCX) * (x - mCX) + (y - mCY) * (y - mCY) <= mR * mR * 2.4f; }
  bool IsDirty() override { if (mLook && ++mTick >= 10) { mTick = 0; return true; } return IKnobControlBase::IsDirty(); }
private:
  int mTick = 0;
  void Report() { if (const IParam* p = GetParam()) { char b[96]; snprintf(b, sizeof(b), "%s  %.1f", mInfo.c_str(), p->Value()); SetInfo(b); } }
  float mCX, mCY, mR; std::string mLabel, mInfo, mFn; bool mAbove; std::vector<std::string> mZones; std::function<Look()> mLook;
};

// ---------------------------------------------------------------- rotary switch (panel.py rotary())
class PRotary : public IKnobControlBase
{
public:
  // marks: "w:tri" draws a waveform symbol, anything else is printed
  PRotary(float cx, float cy, int param, const char* label, const char* info, std::vector<std::string> marks)
  : IKnobControlBase(IRECT(cx - 6.2f * S, cy - 3.2f * 2.2f * S, cx + 6.2f * S, cy + 8.f * S), param, EDirection::Vertical, 0.8),
    mCX(cx), mCY(cy), mR(3.0f * 1.05f * S), mLabel(label), mInfo(info), mMarks(marks) {}
  void Draw(IGraphics& g) override
  {
    const int n = (int)mMarks.size();
    for (int i = 0; i < n; i++)
    {
      const float a = Rad(140.f - i * (100.f / (n - 1)));
      g.DrawLine(kInk, mCX + mR * 1.1f * std::cos(a), mCY - mR * 1.1f * std::sin(a), mCX + mR * 1.3f * std::cos(a), mCY - mR * 1.3f * std::sin(a), nullptr, 0.25f * S);
      const float mx = mCX + mR * 1.75f * std::cos(a), my = mCY - mR * 1.75f * std::sin(a);
      const IColor c = (i == Sel()) ? kAccent : kInk;
      if (mMarks[i].rfind("w:", 0) == 0) Wave(g, mMarks[i].c_str() + 2, mx, my, mR * 0.52f, c, 1.3f);
      else Txt(g, mx, my + mR * 0.18f, mMarks[i].c_str(), mR * 0.48f / S, c);
    }
    g.FillCircle(kPaper, mCX, mCY, mR);
    g.DrawCircle(kInk, mCX, mCY, mR, nullptr, 0.55f * S);
    const float a = Rad(140.f - Sel() * (100.f / (n - 1)));
    IStrokeOptions o; o.mCapOption = ELineCap::Round;
    g.PathMoveTo(mCX + mR * 0.1f * std::cos(a), mCY - mR * 0.1f * std::sin(a));
    g.PathLineTo(mCX + mR * 0.86f * std::cos(a), mCY - mR * 0.86f * std::sin(a));
    g.PathStroke(IPattern(kInk), mR * 0.13f, o);
    Txt(g, mCX, mCY + (mR / S * 1.25f + 1.7f) * S + 0.3f * S, mLabel.c_str(), 1.72f);
  }
  void OnMouseDown(float x, float y, const IMouseMod& m) override { mDragged = false; IKnobControlBase::OnMouseDown(x, y, m); }
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& m) override { mDragged = true; IKnobControlBase::OnMouseDrag(x, y, dX, dY, m); }
  void OnMouseUp(float x, float y, const IMouseMod& m) override
  {
    IKnobControlBase::OnMouseUp(x, y, m);
    if (!mDragged && GetParam())   // a click steps to the next position
    {
      SetValue(GetParam()->ToNormalized((double)((Sel() + 1) % (int)mMarks.size())));
      GetDelegate()->BeginInformHostOfParamChangeFromUI(GetParamIdx());
      SetDirty(true);
      GetDelegate()->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }
  }
  void OnMouseOver(float x, float y, const IMouseMod& m) override { IKnobControlBase::OnMouseOver(x, y, m); SetInfo(mInfo); SetDirty(false); }
  void OnMouseOut() override { IKnobControlBase::OnMouseOut(); SetDirty(false); }
  bool IsHit(float x, float y) const override { return (x - mCX) * (x - mCX) + (y - mCY) * (y - mCY) <= mR * mR * 3.2f; }
private:
  int Sel() const { const IParam* p = GetParam(); return p ? std::max(0, std::min((int)mMarks.size() - 1, (int)std::lround(p->Value()))) : 0; }
  float mCX, mCY, mR; std::string mLabel, mInfo; std::vector<std::string> mMarks; bool mDragged = false;
};

// ---------------------------------------------------------------- ESP knobs
// Plain knobs whose name follows the play mode, as on the hardware: in poly they run the delay, in VCO2 arp / latch
// LOW CUT, HIGH CUT and CV ADJUST run the arp (RATE, OCTAVES, ORDER). Holding SWITCH brings the delay back, and
// THRESHOLD becomes the delay TONE. Only the name under the knob changes; nothing else is shown.
static bool& SwitchHeld() { static bool b = false; return b; }
static int ArpZone(IEditorDelegate* d)   // engine zones of SIGNAL LEVEL: < 18 poly, < 42 VCO2 arp, else latch
{
  if (!d) return 0;
  const double mo = d->GetParam(PARAM_INDEX_MS20__Voices_arpmode)->Value();
  return mo < 18.0 ? 0 : (mo < 42.0 ? 1 : 2);
}

class EspKnob : public IKnobControlBase
{
public:
  struct Job { int param; const char* name; };
  EspKnob(float cx, float cy, const char* hw, Job delay, Job arp, Job sw)
  : IKnobControlBase(IRECT(cx - 6.8f * S, cy - 3.2f * 2.4f * S, cx + 6.8f * S, cy + 3.2f * 1.62f * S + 6.f * S), delay.param, EDirection::Vertical, 2.0),
    mCX(cx), mCY(cy), mR(3.2f * S), mHw(hw) { mJob[0] = delay; mJob[1] = arp; mJob[2] = sw; }

  bool IsDirty() override
  {
    if (mJob[0].param == PARAM_INDEX_MS20__Voices_arpmode)   // the label follows the knob's own zone
    { const int z = ArpZone(GetDelegate()); if (z != mZoneShown) { mZoneShown = z; SetDirty(false); } }
    int j = 0;
    if (SwitchHeld()) j = mJob[2].param >= 0 ? 2 : 0;
    else if (ArpZone(GetDelegate()) > 0 && mJob[1].param >= 0) j = 1;
    if (j != mActive)
    {
      mActive = j;
      SetParamIdx(mJob[j].param);
      SetValue(GetDelegate()->GetParam(mJob[j].param)->GetNormalized());
      SetDirty(false);
    }
    return IKnobControlBase::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    for (int i = 0; i < 11; i++)
    {
      const float a = Rad(240.f - 30.f * i);
      g.DrawLine(kInk, mCX + mR * 1.12f * std::cos(a), mCY - mR * 1.12f * std::sin(a), mCX + mR * 1.5f * std::cos(a), mCY - mR * 1.5f * std::sin(a), nullptr, 0.3f * S);
    }
    g.FillCircle(kPaper, mCX, mCY, mR);
    g.DrawCircle(kInk, mCX, mCY, mR, nullptr, 0.38f * S);
    const float v = (float)GetValue(), a = Rad(240.f - 300.f * std::max(0.f, std::min(1.f, v)));
    IStrokeOptions o; o.mCapOption = ELineCap::Round;
    g.PathMoveTo(mCX + mR * 0.1f * std::cos(a), mCY - mR * 0.1f * std::sin(a));
    g.PathLineTo(mCX + mR * 0.86f * std::cos(a), mCY - mR * 0.86f * std::sin(a));
    g.PathStroke(IPattern(kInk), mR * 0.13f, o);
    // the name under the knob: what it does now (play mode: the mode itself). The same baseline for all five.
    static const char* kModes[3] = { "POLY", "VCO2 ARP", "LATCH" };
    const bool isMode = mJob[0].param == PARAM_INDEX_MS20__Voices_arpmode;
    Txt(g, mCX, mCY + (mR / S * 1.62f + 1.5f) * S + 0.4f * S, isMode ? kModes[ArpZone(GetDelegate())] : mJob[mActive].name, 1.72f);
    // play mode: a thin arc in three parts (poly, VCO2 arp, latch), the current one solid, the others dotted
    if (mJob[0].param == PARAM_INDEX_MS20__Voices_arpmode)
    {
      const int zone = ArpZone(GetDelegate());
      const float rr = mR * 1.75f, edges[4] = {0.f, 18.f / 64.f, 42.f / 64.f, 1.f};
      for (int z = 0; z < 3; z++)
      {
        const float f0 = edges[z] + (z ? 0.025f : 0.f), f1 = edges[z + 1] - (z < 2 ? 0.025f : 0.f);
        if (z == zone) g.DrawArc(kInk, mCX, mCY, rr, -150.f + 300.f * f0, -150.f + 300.f * f1, nullptr, 2.f);   // degrees clockwise from 12 o'clock
        else
          for (float f = f0; f <= f1 + 1e-4f; f += 0.03f)
          {
            const float aa = Rad(240.f - 300.f * f);
            g.FillCircle(kInk, mCX + rr * std::cos(aa), mCY - rr * std::sin(aa), 1.1f);
          }
      }
    }
  }
  void OnMouseOver(float x, float y, const IMouseMod& m) override { IKnobControlBase::OnMouseOver(x, y, m); Report(); }
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& m) override { IKnobControlBase::OnMouseDrag(x, y, dX, dY, m); Report(); }
  bool IsHit(float x, float y) const override { return (x - mCX) * (x - mCX) + (y - mCY) * (y - mCY) <= mR * mR * 2.4f; }
private:
  void Report()
  {
    const IParam* p = GetParam(); if (!p) return;
    static const char* kModes[3] = { "POLY", "VCO2 ARP", "LATCH" };
    const char* nm = mJob[0].param == PARAM_INDEX_MS20__Voices_arpmode ? kModes[ArpZone(GetDelegate())] : mJob[mActive].name;
    char b[128]; snprintf(b, sizeof(b), "%s  \xC2\xB7  %s  %.1f", mHw, nm, p->Value());
    SetInfo(b);
  }
  float mCX, mCY, mR; const char* mHw;
  Job mJob[3]; int mActive = 0, mZoneShown = -1;
};

// ---------------------------------------------------------------- patch panel (jacks at the photo-grid positions)
// kind: 1 output (src = engine source), 2 input (a PatchBay parameter), 3 mono gesture (VCO 1+2 CV IN: KBD CV only),
//       4 tap gesture (ESP SIGNAL IN: SWITCH only), 0 not used by the engine yet (grey)
struct Jack { int id; float px, py; const char* label; int kind; int src; int param; };
static const Jack kJacks[35] = {
  {0, 235, 310, "TOTAL", 2, 0, PARAM_INDEX_PatchBay_total}, {5, 410, 310, "FREQ", 2, 0, PARAM_INDEX_PatchBay_freq},
  {9, 560, 310, "EXT\nSIGNAL IN", 0, 0, -1}, {13, 700, 310, "HPF\nCUTOFF", 2, 0, PARAM_INDEX_PatchBay_hpf},
  {17, 850, 310, "LPF\nCUTOFF", 2, 0, PARAM_INDEX_PatchBay_lpf}, {21, 1005, 310, "INITIAL\nGAIN", 0, 0, -1},
  {29, 1405, 185, "SIGNAL OUT", 0, 0, -1}, {30, 1405, 315, "PHONES", 0, 0, -1},
  {1, 235, 465, "MG TRI\nOUT", 1, 1, -1}, {6, 490, 465, "MG SQ\nOUT", 1, 2, -1}, {10, 610, 465, "EG1\nOUT", 1, 3, -1},
  {22, 1070, 465, "EG2\nREV OUT", 1, 6, -1}, {24, 1160, 555, "VCO2\nCV IN", 2, 0, PARAM_INDEX_PatchBay_vco2freq},
  {25, 1280, 480, "VCO1+2\nCV IN", 3, 0, -1}, {31, 1400, 480, "KBD\nCV OUT", 1, 11, -1},
  {4, 365, 590, "S&H\nCLOCK", 2, 0, PARAM_INDEX_PatchBay_shclock}, {11, 610, 590, "EG1\nREV OUT", 1, 4, -1},
  {14, 725, 590, "VCA\nCTRL IN", 2, 0, PARAM_INDEX_PatchBay_vca}, {18, 915, 590, "EG1\nTRIG IN", 2, 0, PARAM_INDEX_PatchBay_eg1trig},
  {26, 1280, 600, "KBD\nTRIG IN", 2, 0, PARAM_INDEX_PatchBay_eg2trig}, {32, 1400, 600, "KBD\nTRIG OUT", 1, 12, -1},
  {2, 245, 710, "S&H IN", 2, 0, PARAM_INDEX_PatchBay_shin}, {7, 490, 710, "S&H OUT", 1, 7, -1}, {12, 610, 710, "VCA IN", 0, 0, -1},
  {16, 795, 710, "VCA OUT", 0, 0, -1}, {19, 915, 710, "PINK", 1, 8, -1}, {23, 1150, 710, "WHITE", 1, 8, -1},
  {27, 1270, 710, "WHEEL", 1, 9, -1}, {33, 1390, 710, "SWITCH", 1, 10, -1},
  {3, 250, 870, "SIGNAL IN", 4, 0, -1}, {8, 490, 870, "AMP OUT", 0, 0, -1}, {15, 730, 870, "BPF OUT", 0, 0, -1},
  {20, 965, 870, "F-V OUT", 0, 0, -1}, {28, 1265, 870, "ENV OUT", 0, 0, -1}, {34, 1385, 870, "TRIG OUT", 0, 0, -1},
};

class PatchPanel : public IControl
{
public:
  PatchPanel(PatchHost* host) : IControl(IRECT(X(LW), Yd(0), X(PW), Yd(PH))), mHost(host) {}
  static void JackXY(int i, float& x, float& y) { x = RX(kJacks[i].px); y = RY(kJacks[i].py); }
  static int IndexOfId(int id) { for (int i = 0; i < 35; i++) if (kJacks[i].id == id) return i; return -1; }
  int HitJack(float x, float y) const
  {
    for (int i = 0; i < 35; i++) { float jx, jy; JackXY(i, jx, jy); if ((x - jx) * (x - jx) + (y - jy) * (y - jy) < 2.2f * S * 2.2f * S) return i; }
    return -1;
  }
  bool IsHit(float x, float y) const override { return HitJack(x, y) >= 0 || mDragFrom >= 0; }

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
    auto* d = GetDelegate();
    const float r = 1.5f * S;
    for (int i = 0; i < 35; i++)
    {
      const Jack& j = kJacks[i];
      float x, y; JackXY(i, x, y);
      const bool hot = (mHover == i || mDragFrom == i) && j.kind != 0;
      g.FillCircle(kPaper, x, y, r * 1.3f);
      if (j.kind == 1)        // output: a solid black ring
      {
        g.FillCircle(kInk, x, y, r * 1.3f);
        g.FillCircle(kPaper, x, y, r * 0.75f);
        if (hot) g.DrawCircle(kInk, x, y, r * 1.6f, nullptr, 1.5f);
      }
      else if (j.kind != 0)   // input: a thin ring
      {
        g.DrawCircle(kInk, x, y, r * 1.3f, nullptr, (hot ? 0.6f : 0.35f) * S);
        g.DrawCircle(kInk, x, y, r * 0.75f, nullptr, 0.25f * S);
      }
      else g.DrawCircle(kGrey, x, y, r * 1.2f, nullptr, 0.3f * S);   // not used: grey
      const int n = Lines(j.label);
      Txt(g, x, y - (r * 1.3f + 0.8f * S + 1.75f * S * (n - 1)) - 0.1f * S, j.label, 1.75f, j.kind != 0 ? kInk : kDimC);
      // a plug with its far end loose (patched like that on the MS-20ic): the normal is broken, nothing comes in
      if (j.kind == 2 && d && (int)std::lround(d->GetParam(j.param)->Value()) == 13) g.FillCircle(kInk, x, y, r * 0.7f);
    }
    for (const Cable& cb : Cables())
    {
      float x1, y1, x2, y2; JackXY(cb.out, x1, y1); JackXY(cb.in, x2, y2);
      DrawCable(g, x1, y1, x2, y2, kInk);
    }
    if (mDragFrom >= 0) { float x1, y1; JackXY(mDragFrom, x1, y1); DrawCable(g, x1, y1, mDragX, mDragY, kGrey); }
  }

  void DrawCable(IGraphics& g, float x1, float y1, float x2, float y2, const IColor& col)
  {
    const float dist = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)), sag = 12.f + dist * 0.28f;
    IStrokeOptions o; o.mCapOption = ELineCap::Round;
    g.PathMoveTo(x1, y1); g.PathCubicBezierTo(x1, y1 + sag, x2, y2 + sag, x2, y2); g.PathStroke(IPattern(kPaper), 1.25f * S, o);
    g.PathMoveTo(x1, y1); g.PathCubicBezierTo(x1, y1 + sag, x2, y2 + sag, x2, y2); g.PathStroke(IPattern(col), 0.7f * S, o);
    g.FillCircle(col, x1, y1, 0.9f * S); g.FillCircle(col, x2, y2, 0.9f * S);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const int j = HitJack(x, y);
    if (j < 0 || kJacks[j].kind == 0) return;
    mDragX = x; mDragY = y;
    for (const Cable& cb : Cables()) if (cb.in == j) { mDragFrom = cb.out; Disconnect(j); SetDirty(false); return; }   // pick the cable up by its plug
    mDragFrom = j; SetDirty(false);
  }
  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override { if (mDragFrom >= 0) { mDragX = x; mDragY = y; mHover = HitJack(x, y); SetDirty(false); } }
  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    if (mDragFrom < 0) return;
    const int a = mDragFrom, b = HitJack(x, y);
    mDragFrom = -1;
    if (b >= 0 && b != a) Connect(a, b);   // dropped anywhere else: the cable is out
    SetDirty(false);
  }
  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int h = HitJack(x, y);
    if (h != mHover) { mHover = h; SetDirty(false); }
    if (h >= 0)
    {
      std::string s(kJacks[h].label); for (char& c : s) if (c == '\n') c = ' ';
      static const char* kWhat[5] = { "  \xC2\xB7  not used yet", "  \xC2\xB7  output", "  \xC2\xB7  input", "  \xC2\xB7  patch KBD CV OUT here for mono", "  \xC2\xB7  patch SWITCH here for tap tempo" };
      SetInfo(s + kWhat[kJacks[h].kind]);
    }
  }
  void OnMouseOut() override { mHover = -1; SetDirty(false); }
  bool IsDirty() override { if (++mTick >= 15) { mTick = 0; return true; } return IControl::IsDirty(); }   // cables patched on the MS-20ic

private:
  void SetInput(int param, int src)
  {
    auto* d = GetDelegate(); if (!d) return;
    d->BeginInformHostOfParamChangeFromUI(param);
    d->SendParameterValueFromUI(param, d->GetParam(param)->ToNormalized((double)src));
    d->EndInformHostOfParamChangeFromUI(param);
  }
  void Disconnect(int in)
  {
    const Jack& j = kJacks[in];
    if (j.kind == 2) SetInput(j.param, 0); else if (j.kind == 3) mHost->SetCableFlag(0, false); else if (j.kind == 4) mHost->SetCableFlag(1, false);
  }
  void Connect(int a, int b)
  {
    int out = a, in = b;
    if (kJacks[out].kind != 1) std::swap(out, in);
    if (kJacks[out].kind != 1 || kJacks[in].kind < 2) return;   // output to output, or a jack that does nothing yet
    const Jack& o = kJacks[out]; const Jack& i = kJacks[in];
    if (i.kind == 2) { if (o.src == 8) mHost->SetNoisePick(in, o.id == 23 ? 1 : 0); SetInput(i.param, o.src); }
    else if (i.kind == 3 && o.id == 31) mHost->SetCableFlag(0, true);   // the engine's mono mode: KBD CV only
    else if (i.kind == 4 && o.id == 33) mHost->SetCableFlag(1, true);   // tap tempo: SWITCH only
  }
  PatchHost* mHost;
  int mDragFrom = -1, mHover = -1, mTick = 0;
  float mDragX = 0.f, mDragY = 0.f;
};

// ---------------------------------------------------------------- status bar: controller, modes, switch, where the sound is kept
// SWITCH is the MS-20's momentary switch (tap tempo with a cable SWITCH -> ESP SIGNAL IN).
class StatusBar : public IControl
{
public:
  StatusBar(const IRECT& r, PatchHost* ph, std::function<int()> controller, std::function<void(char*, int)> diag)
  : IControl(r), mPatch(ph), mCtl(controller), mDiag(diag) {}
  IRECT SwitchB() const { return IRECT(mRECT.L + 640.f, mRECT.T + 8.f, mRECT.L + 728.f, mRECT.B - 8.f); }

  void Draw(IGraphics& g) override
  {
    g.FillRect(kBar, mRECT);
    g.DrawLine(kInk, mRECT.L, mRECT.T, mRECT.R, mRECT.T, nullptr, 1.f);
    const float cy = mRECT.MH();
    static const char* kNames[] = { "", "MS-20ic", "Ksoloti Core", "Teensy", "microKONTROL" };
    const int c = mCtl ? mCtl() : 0;
    if (c > 0 && c <= 4) { g.FillCircle(kInk, mRECT.L + 22.f, cy, 6.f); g.DrawText(IText(15.f, kInk, kF, EAlign::Near, EVAlign::Middle), (std::string("Connected  \xC2\xB7  ") + kNames[c]).c_str(), IRECT(mRECT.L + 36.f, mRECT.T, mRECT.L + 224.f, mRECT.B)); }
    else { g.DrawCircle(kGrey, mRECT.L + 22.f, cy, 6.f, nullptr, 1.5f); g.DrawText(IText(15.f, kGrey, kF, EAlign::Near, EVAlign::Middle), "Any MIDI keyboard", IRECT(mRECT.L + 36.f, mRECT.T, mRECT.L + 224.f, mRECT.B)); }
    // what the mouse is over, else where the sound is kept
    Info& in = HoverInfo();
    if (!in.text.empty() && in.age < 90) g.DrawText(IText(14.f, kInk, kF, EAlign::Near, EVAlign::Middle), in.text.c_str(), IRECT(mRECT.L + 236.f, mRECT.T, mRECT.L + 630.f, mRECT.B));
    else { char d[200] = ""; if (mDiag) mDiag(d, sizeof(d)); g.DrawText(IText(13.f, kGrey, kF, EAlign::Near, EVAlign::Middle), d, IRECT(mRECT.L + 236.f, mRECT.T, mRECT.L + 630.f, mRECT.B)); }
    // modes
    const double mo = GetDelegate() ? GetDelegate()->GetParam(PARAM_INDEX_MS20__Voices_arpmode)->Value() : 0.0;
    std::string m = mPatch->CableFlag(0) ? "mono" : "poly";
    m += mo < 18.0 ? "  \xC2\xB7  arp off" : (mo < 42.0 ? "  \xC2\xB7  arp" : "  \xC2\xB7  latch");
    if (mPatch->CableFlag(1)) m += "  \xC2\xB7  tap tempo";
    g.DrawText(IText(13.f, kGrey, kF, EAlign::Near, EVAlign::Middle), m.c_str(), IRECT(mRECT.L + 740.f, mRECT.T, mRECT.L + 920.f, mRECT.B));
    if (mSwitch) g.FillRect(kAccent, SwitchB());
    g.DrawRect(mSwitch ? kAccent : kInk, SwitchB(), nullptr, 1.5f);
    g.DrawText(IText(12.f, mSwitch ? kPaper : kInk, kF, EAlign::Center, EVAlign::Middle), "SWITCH", SwitchB());   // black while held
    g.DrawText(IText(17.f, kInk, kFB, EAlign::Far, EVAlign::Middle), "TWENTY ENGINE", IRECT(mRECT.R - 260.f, mRECT.T, mRECT.R - 16.f, mRECT.B));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (SwitchB().Contains(x, y)) { mSwitch = true; SwitchHeld() = true; mPatch->SwitchDown(true); SetDirty(false); }
  }
  void OnMouseUp(float, float, const IMouseMod&) override { if (mSwitch) { mSwitch = false; SwitchHeld() = false; mPatch->SwitchDown(false); SetDirty(false); } }
  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    if (SwitchB().Contains(x, y)) SetInfo("SWITCH  \xC2\xB7  the MS-20's momentary switch  \xC2\xB7  tap: tempo (SWITCH -> ESP SIGNAL IN)");
  }
  bool IsDirty() override { HoverInfo().age++; if (++mTick >= 6) { mTick = 0; return true; } return IControl::IsDirty(); }
private:
  PatchHost* mPatch; std::function<int()> mCtl; std::function<void(char*, int)> mDiag;
  int mTick = 0; bool mSwitch = false;
};

// ---------------------------------------------------------------- the whole window
inline void Layout(IGraphics* g, PatchHost* ph, std::function<int()> controller, std::function<void(char*, int)> diag)
{
  g->AttachControl(new PanelArt());
  g->AttachControl(new LedLights(ph));
  static const float COLX[8] = {7.5f, 20.0f, 32.6f, 45.0f, 57.4f, 69.8f, 82.2f, 94.0f};
  const float RA = 18.f, RB = 35.f, RC = 51.f, RD = 71.f, MA = 58.5f, MB = 71.f;
  const float EGR[5] = {17.f, 31.f, 45.f, 58.5f, 71.f};
  auto knob = [&](int col, float yd, int param, const char* label, const char* info) { g->AttachControl(new PKnob(X(COLX[col]), Yd(yd), 3.0f, param, label, info)); };
  // VCO 1 / VCO 2 (rotaries: the waveform symbols are the sounds the engine makes in each position)
  g->AttachControl(new PRotary(X(COLX[0]), Yd(RA), PARAM_INDEX_MS20__Voices_vco1wave, "WAVEFORM", "VCO 1 WAVEFORM  \xC2\xB7  PULSE WIDTH shapes every waveform", {"w:tri", "w:saw", "w:pulse", "w:noise"}));
  {
    PKnob* pw = new PKnob(X(COLX[0]), Yd(RB), 3.0f, PARAM_INDEX_MS20__Voices_pw, "PULSE\nWIDTH", "VCO 1 PULSE WIDTH");
    pw->SetLook([g]() -> PKnob::Look {   // what PW does depends on VCO 1's waveform
      const int w = (int)std::lround(g->GetDelegate()->GetParam(PARAM_INDEX_MS20__Voices_vco1wave)->Value());
      switch (w)
      {
        case 0: return {"SINE \xE2\x86\x92 TRIANGLE", "VCO 1  \xC2\xB7  PW morphs the sine into a triangle", 0};
        case 1: return {"PHASER", "VCO 1  \xC2\xB7  PW is the phaser depth on the saw (the MG sweeps it)", 1};
        case 3: return {"WHITE \xE2\x86\x92 PINK", "VCO 1  \xC2\xB7  PW turns white noise into pink", 3};
        default: return {"PULSE WIDTH", "VCO 1  \xC2\xB7  PULSE WIDTH", 2};
      }
    });
    g->AttachControl(pw);
  }
  g->AttachControl(new PRotary(X(COLX[0]), Yd(RC), PARAM_INDEX_MS20__Voices_vco1oct, "SCALE", "VCO 1 SCALE", {"32'", "16'", "8'", "4'"}));
  knob(0, RD, PARAM_INDEX_MS20__Voices_portamento, "TIME", "PORTAMENTO  \xC2\xB7  Multibend glide in poly");
  g->AttachControl(new PRotary(X(COLX[1]), Yd(RA), PARAM_INDEX_MS20__Voices_vco2wave, "WAVEFORM", "VCO 2 WAVEFORM", {"w:tri", "w:saw", "w:sq", "w:ring"}));
  knob(1, RB, PARAM_INDEX_MS20__Voices_vco2semi, "PITCH", "VCO 2 PITCH");
  g->AttachControl(new PRotary(X(COLX[1]), Yd(RC), PARAM_INDEX_MS20__Voices_vco2oct, "SCALE", "VCO 2 SCALE", {"16'", "8'", "4'", "2'"}));
  knob(1, RD, PARAM_INDEX_MS20__Master__Tune_tune, "", "MASTER TUNE");
  // mixer, filters, modulation
  knob(2, RA, PARAM_INDEX_Master__Mixer_vco1, "VCO 1\nLEVEL", "VCO 1 LEVEL");
  knob(2, RB, PARAM_INDEX_Master__Mixer_vco2, "VCO 2\nLEVEL", "VCO 2 LEVEL");
  knob(2, MA, PARAM_INDEX_MG__Morphing__LFO_pitchmod, "MG/T.EXT", "FREQ MOD  \xC2\xB7  MG/T.EXT");
  knob(2, MB, PARAM_INDEX_EG1_amount, "EG1/EXT", "FREQ MOD  \xC2\xB7  EG1/EXT");
  knob(3, RA, PARAM_INDEX_Filter__Macro_hpfcut, "CUTOFF\nFREQUENCY", "HPF CUTOFF FREQUENCY");
  knob(3, RB, PARAM_INDEX_Filter__Macro_hpfpeak, "PEAK", "HPF PEAK");
  knob(3, MA, PARAM_INDEX_MG__Morphing__LFO_hpfmod, "MG/T.EXT", "HPF MOD  \xC2\xB7  MG/T.EXT");
  knob(3, MB, PARAM_INDEX_EG2_hpfdepth, "EG2/EXT", "HPF MOD  \xC2\xB7  EG2/EXT");
  knob(4, RA, PARAM_INDEX_Filter__Macro_lpfcut, "CUTOFF\nFREQUENCY", "LPF CUTOFF FREQUENCY");
  knob(4, RB, PARAM_INDEX_Filter__Macro_lpfpeak, "PEAK", "LPF PEAK");
  knob(4, MA, PARAM_INDEX_MG__Morphing__LFO_lpfmod, "MG/T.EXT", "LPF MOD  \xC2\xB7  MG/T.EXT");
  knob(4, MB, PARAM_INDEX_EG2_lpfdepth, "EG2/EXT", "LPF MOD  \xC2\xB7  EG2/EXT");
  // MG, EG 1, EG 2
  knob(5, MA, PARAM_INDEX_MG__Morphing__LFO_shape, "WAVEFORM", "MG WAVEFORM  \xC2\xB7  triangle, square, smooth random");
  knob(5, MB, PARAM_INDEX_MG__Morphing__LFO_rate, "FREQUENCY", "MG FREQUENCY");
  knob(6, 46.f, PARAM_INDEX_EG1_delay, "DELAY", "EG 1 DELAY TIME");
  knob(6, MA, PARAM_INDEX_EG1_attack, "ATTACK", "EG 1 ATTACK TIME");
  knob(6, MB, PARAM_INDEX_EG1_release, "RELEASE", "EG 1 RELEASE TIME");
  knob(7, EGR[0], PARAM_INDEX_EG2_hold, "HOLD", "EG 2 HOLD TIME  \xC2\xB7  fully up = chord latch in poly");
  knob(7, EGR[1], PARAM_INDEX_EG2_attack, "ATTACK", "EG 2 ATTACK TIME");
  knob(7, EGR[2], PARAM_INDEX_EG2_decay, "DECAY", "EG 2 DECAY TIME");
  knob(7, EGR[3], PARAM_INDEX_EG2_sustain, "SUSTAIN", "EG 2 SUSTAIN LEVEL");
  knob(7, EGR[4], PARAM_INDEX_EG2_release, "RELEASE", "EG 2 RELEASE TIME");
  // patch panel (jacks + cables), volume
  g->AttachControl(new PatchPanel(ph));
  g->AttachControl(new PKnob(RX(1210), RY(280), 4.2f, PARAM_INDEX_VCA__Macro_mastervol, "", "VOLUME"));
  // ESP: play mode, and four knobs whose names follow it
  const float ey = RY(1010);
  const EspKnob::Job none = {-1, ""};
  g->AttachControl(new EspKnob(RX(270), ey, "SIGNAL LEVEL", {PARAM_INDEX_MS20__Voices_arpmode, "POLY"}, none, none));
  g->AttachControl(new EspKnob(RX(460), ey, "LOW CUT FREQ", {PARAM_INDEX_BBD__Delay_time, "TIME"}, {PARAM_INDEX_MS20__Voices_arprate, "RATE"}, none));
  g->AttachControl(new EspKnob(RX(640), ey, "HIGH CUT FREQ", {PARAM_INDEX_BBD__Delay_feedback, "FEEDBACK"}, {PARAM_INDEX_MS20__Voices_arpoct, "OCTAVES"}, none));
  g->AttachControl(new EspKnob(RX(820), ey, "CV ADJUST", {PARAM_INDEX_BBD__Delay_wowflutter, "WOW / FLUTTER"}, {PARAM_INDEX_MS20__Voices_arporder, "ORDER"}, none));
  g->AttachControl(new EspKnob(RX(1000), ey, "THRESHOLD LEVEL", {PARAM_INDEX_BBD__Delay_drywet, "DELAY AMOUNT"}, none, {PARAM_INDEX_BBD__Delay_bbdfilter, "TONE"}));
  // status bar
  g->AttachControl(new StatusBar(IRECT(0, Yd(PH) + 10.f, (float)kW, Yd(PH) + 10.f + BARH), ph, controller, diag));
  g->AttachCornerResizer(EUIResizerMode::Scale, false);
}

} // namespace te_ui
