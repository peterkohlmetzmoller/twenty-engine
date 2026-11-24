// dsp.cpp - Twenty Engine in the browser, the sound: the same engine as every edition (engine/ms20_host.h,
// generated from the Ksoloti patch), built as a standalone WebAssembly module and run by worklet.js in the
// browser's audio thread. Plain C functions, no JavaScript glue.
// The 36 sound memories live in a RAM file in the MS20MEM.BIN format of the computer editions; the page keeps it
// in the browser's storage (loads it at the start, stores it after a save on the keys).
#include <cstdint>
#include <cstring>
#include "../engine/engine_gen.h"
#include "../engine/ms20_host.h"
using namespace ms20;
#define TE_EXPORT extern "C" __attribute__((used, visibility("default")))

static MS20Host* gHost = nullptr;
static float gL[1024], gR[1024];
static uint8_t gLed[16];

TE_EXPORT void te_init(double sampleRate)
{
  if (!gHost) gHost = new MS20Host();
  gHost->reset(sampleRate);
  gHost->pinParam(PARAM_INDEX_BBD__Delay_bbdfilter, 67108864);   // BBD tone held in the middle (as in the plugin window)
}
TE_EXPORT int te_nparams() { return NPARAMS; }
TE_EXPORT float* te_left() { return gL; }
TE_EXPORT float* te_right() { return gR; }
// one block for the browser (128 frames as a rule)
TE_EXPORT void te_process(int n) { if (!gHost) return; if (n > 1024) n = 1024; gHost->process(gL, gR, n); ParamSet p; while (gHost->popEngineChange(p)) {} }
TE_EXPORT void te_midi(int s, int d1, int d2) { if (gHost) gHost->midi(0, (uint8_t)s, (uint8_t)d1, (uint8_t)d2); }
TE_EXPORT int te_get(int i) { return (gHost && i >= 0 && i < NPARAMS) ? gHost->getParamRaw(i) : 0; }
TE_EXPORT void te_set(int i, int raw) { if (gHost) gHost->setParamRaw(i, raw); }
TE_EXPORT void te_light_show() { if (gHost) gHost->requestLightShow(); }
// LED SysEx for the MS-20ic (and the panel's LEDs): one message per call, 0 = none left
TE_EXPORT uint8_t* te_led_buf() { return gLed; }
TE_EXPORT int te_led_pop() { LedMsg m; if (!gHost || !gHost->popLed(m)) return 0; memcpy(gLed, m.b, m.len); return m.len; }
TE_EXPORT int te_mem_slot() { return gHost ? gHost->memSlot() : -1; }
// the memory file (MS20MEM.BIN) in RAM
TE_EXPORT uint8_t* te_mem_ptr() { return g_ramFile; }
TE_EXPORT int te_mem_cap() { return (int)sizeof(g_ramFile); }
TE_EXPORT int te_mem_load(int len) { if (len < 0 || len > (int)sizeof(g_ramFile)) return 0; g_ramFileLen = (uint32_t)len; return (gHost && gHost->loadMemories("")) ? 1 : 0; }
// after a save on the keys: the file's new length (the page stores it), else 0
TE_EXPORT int te_mem_poll() { if (!gHost) return 0; gHost->pollMemorySave(); if (g_ramFileDirty) { g_ramFileDirty = false; return (int)g_ramFileLen; } return 0; }
