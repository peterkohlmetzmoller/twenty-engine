#include "TwentyEngine.h"
#include "IPlug_include_in_plug_src.h"
#include <string.h>

using namespace ms20;

// parameter type helpers (types as written in the Ksoloti patch)
static bool IsFrac(int i) { return strncmp(kParamInfo[i].type, "frac32", 6) == 0; }
static bool IsBipolar(int i) { return strncmp(kParamInfo[i].type, "frac32.s", 8) == 0; }
static bool IsBool(int i) { return strncmp(kParamInfo[i].type, "bool32", 6) == 0; }

TwentyEngine::TwentyEngine(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
#if IPLUG_DSP
  mHost = std::make_unique<MS20Host>();
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

void TwentyEngine::OnReset()
{
  mHost->reset(GetSampleRate());
  // the reset keeps the engine's settings; make sure the plugin's current values are applied
  for (int i = 0; i < kNumParams; i++) mHost->setParamRaw(i, ToRaw(i, GetParam(i)->Value()));
}

void TwentyEngine::ProcessMidiMsg(const IMidiMsg& msg)
{
  // everything goes to the engine: notes, CCs (knobs, rotaries, patch bay NRPN), channel pressure (button)
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
  if (mFromEngine) return;   // the engine already has this value (it made the change)
  mHost->setParamRaw(paramIdx, ToRaw(paramIdx, GetParam(paramIdx)->Value()));
}

void TwentyEngine::OnIdle()
{
  // main thread, about every 20 ms
  // 1. changes made by the engine (iC knobs via CC, power-on dump, ESP knob layers, tap tempo, patch bay)
  ParamSet p;
  while (mHost->popEngineChange(p))
  {
    if (p.idx < 0 || p.idx >= kNumParams) continue;
    const double v = FromRaw(p.idx, p.raw);
    mFromEngine = true;
    BeginInformHostOfParamChangeFromUI(p.idx);
    SetParameterValue(p.idx, GetParam(p.idx)->ToNormalized(v));
    EndInformHostOfParamChangeFromUI(p.idx);
    mFromEngine = false;
  }
  // 2. LEDs on the iC
  if (--mRescan <= 0) { mLed.Rescan(); mRescan = 100; }   // every ~2 s: finds the iC when it is plugged in later
  LedMsg m;
  while (mHost->popLed(m)) mLed.Send(m.b, m.len);
}
#endif
