// Twenty Engine - Teensy 4.1 + SGTL5000 audio board edition
// Copyright (C) Peter Kohlmetz Moller. GPL v3 (see LICENSE in the repository).
//
// The same engine as the Ksoloti patch and the plugin (engine_gen.h, generated from the .axp),
// in the HQ form: oversampled oscillators and arp filter, triangle correction, analog drift.
//
//  - MS-20 iC on the USB host port: keys, knobs, switches, patch cables, LED feedback
//  - audio out: the audio board's headphone and line out, 48 kHz, dithered to 16 bit
//  - audio in: line in (left) -> the engine's external input (EXT SIGNAL)
//  - patch memory on the keys: saved in the Teensy's own flash (no SD card needed);
//    an SD card in the Teensy's slot, if present, gets a backup copy (MS20MEM.BIN)
//  - USB device (Tools > USB Type > "Serial + MIDI"): shows up as "Twenty Engine";
//    everything from the iC is passed through (memory keys as CC 102/103, silent in a DAW),
//    notes/CC from the computer play the engine, SysEx from the computer goes to the iC
//
// Serial Monitor prints the engine's CPU load every 2 seconds.

// ================= SETTINGS =================
#define MS20_OSF 2          // oscillators: 2 = 96 kHz oversampling (cleaner high notes), 1 = as on the Ksoloti (less CPU)
static const float DRIFT = 0.5f;   // analog drift 0.0 .. 1.0: 0 = as on the Ksoloti, 1 = loose vintage poly
static const float HEADPHONE_VOLUME = 0.8f;   // 0.0 .. 1.0
// ============================================

#include <Audio.h>
#include <USBHost_t36.h>
#include <SD.h>
#include <LittleFS.h>
#include <utility/imxrt_hw.h>
#include "engine_gen.h"

using namespace ms20;

static const int SAMPLE_RATE = 48000;          // the engine runs at 48 kHz, like the Ksoloti

// ---------------- USB host: the MS-20 iC ----------------
USBHost myusb;
USBHub hub1(myusb);
MIDIDevice_BigBuffer ic(myusb);

// ---------------- memory storage ----------------
LittleFS_Program flashFS;                       // 256 KB at the top of the Teensy's 8 MB program flash
bool flashOK = false, sdOK = false;

// ---------------- the engine ----------------
EngineCore *core = nullptr;                     // 274 KB, allocated in RAM2 (the BBD delay line is most of it)
volatile bool engineReady = false;
volatile uint32_t cyclesMax = 0;                // longest audio update, in CPU cycles

// MIDI from loop() to the audio interrupt
struct Msg { uint8_t s, d1, d2; };
static Msg midiQ[256];
static volatile uint8_t midiHead = 0, midiTail = 0;
static void queueMidi(uint8_t s, uint8_t d1, uint8_t d2) {
  uint8_t n = midiHead + 1;
  if (n == midiTail) return;                    // full: drop
  midiQ[midiHead] = {s, d1, d2};
  midiHead = n;
}

// LED SysEx from the audio interrupt to loop()
struct Led { uint8_t b[16]; uint8_t len; };
static Led ledQ[64];
static volatile uint8_t ledHead = 0, ledTail = 0;
static void onSysex(const uint8_t *b, int len, void *) {
  uint8_t n = (ledHead + 1) & 63;
  if (n == ledTail) return;
  Led &m = ledQ[ledHead];
  m.len = len > 16 ? 16 : len;
  memcpy(m.b, b, m.len);
  ledHead = n;
}

// ---------------- audio object: runs the engine inside the Teensy audio library ----------------
class TwentyEngineAudio : public AudioStream {
public:
  TwentyEngineAudio() : AudioStream(1, inputQueueArray) {}
  void update(void) override {
    audio_block_t *in = receiveReadOnly(0);
    if (!engineReady) { if (in) release(in); return; }
    audio_block_t *l = allocate(), *r = allocate();
    if (!l || !r) { if (l) release(l); if (r) release(r); if (in) release(in); return; }
    uint32_t t0 = ARM_DWT_CYCCNT;

    while (midiTail != midiHead) {              // MIDI first, like the Ksoloti does each block
      Msg m = midiQ[midiTail];
      core->midi(m.s, m.d1, m.d2);
      midiTail = midiTail + 1;
    }
    for (int b = 0; b < AUDIO_BLOCK_SAMPLES; b += BUFSIZE) {
      for (int i = 0; i < BUFSIZE; i++) core->audioInL[i] = in ? ((int32_t)in->data[b + i]) << 12 : 0;
      core->process();
      for (int i = 0; i < BUFSIZE; i++) {
        l->data[b + i] = to16(core->audioOutL[i]);
        r->data[b + i] = to16(core->audioOutR[i]);
      }
    }

    uint32_t c = ARM_DWT_CYCCNT - t0;
    if (c > cyclesMax) cyclesMax = c;
    transmit(l, 0); transmit(r, 1);
    release(l); release(r);
    if (in) release(in);
  }
private:
  audio_block_t *inputQueueArray[1];
  uint32_t dith = 22222u;
  // frac32 (1<<27 = full scale) -> 16 bit with TPDF dither (+-1 LSB triangular): quiet tails fade smoothly
  inline int16_t to16(int32_t x) {
    dith = dith * 1664525u + 1013904223u; int32_t r1 = (int32_t)(dith >> 20);   // 0..4095
    dith = dith * 1664525u + 1013904223u; int32_t r2 = (int32_t)(dith >> 20);
    int32_t y = (x + r1 + r2 - 2048) >> 12;     // 4096 engine units = 1 LSB; -2048 + 2048 rounding offset keeps it centred
    return (int16_t)(y > 32767 ? 32767 : (y < -32768 ? -32768 : y));
  }
};

AudioInputI2S        lineIn;
TwentyEngineAudio    engine;
AudioOutputI2S       out;
AudioControlSGTL5000 codec;
AudioConnection      c0(lineIn, 0, engine, 0);
AudioConnection      c1(engine, 0, out, 0);
AudioConnection      c2(engine, 1, out, 1);

// Switch the audio board to 48 kHz (the audio library starts at 44.1 kHz).
// Same PLL4 / SAI1 divider formula as the library's own AudioOutputI2S::config_i2s().
void setI2SFreq(int freq) {
  int n1 = 4;                                    // SAI prescaler
  int n2 = 1 + (24000000 * 27) / (freq * 256 * n1);
  double C = ((double)freq * 256 * n1 * n2) / 24000000;
  int c0 = C, c2 = 10000, c1 = C * c2 - (c0 * c2);
  set_audioClock(c0, c1, c2, true);
  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
             | CCM_CS1CDR_SAI1_CLK_PRED(n1 - 1)
             | CCM_CS1CDR_SAI1_CLK_PODF(n2 - 1);
}

// ---------------- patch memory: flash first, SD card as backup ----------------
bool loadMemories() {
  if (flashOK) { g_fs = &flashFS; if (core->o_Patch_Memory.loadFile()) return true; }
  if (sdOK) {                                    // nothing in flash yet: take the SD card's file (e.g. from a Ksoloti) and keep it in flash
    g_fs = &SD;
    if (core->o_Patch_Memory.loadFile()) {
      if (flashOK) { g_fs = &flashFS; core->o_Patch_Memory.saveFile(); }
      return true;
    }
  }
  return false;                                  // factory sounds
}

bool saveMemories() {
  bool ok = false;
  if (flashOK) { g_fs = &flashFS; ok = core->o_Patch_Memory.saveFile(); }
  if (sdOK) { g_fs = &SD; bool s = core->o_Patch_Memory.saveFile(); if (!flashOK) ok = s; }   // backup copy
  return ok;
}

// ---------------- MIDI thru ----------------
bool memArmed = false;                           // momentary switch held = keys pick memory slots

void fromIC(uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2) {
  uint8_t status = type | ((ch - 1) & 0x0F);
  queueMidi(status, d1, d2);                     // to the engine

  if (type == 0xD0) memArmed = (d1 > 0) && !core->o_Patch_Memory.tapmode;
  // to the computer: memory keys become CC 102 (down) / 103 (up) so a DAW doesn't play them
  if (memArmed && type == 0x90 && d2 > 0) usbMIDI.sendControlChange(102, d1, ch);
  else if (memArmed && (type == 0x80 || type == 0x90)) usbMIDI.sendControlChange(103, d1, ch);
  else usbMIDI.send(type, d1, d2, ch, 0);
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  AudioMemory(24);
  setI2SFreq(SAMPLE_RATE);

  codec.enable();
  codec.inputSelect(AUDIO_INPUT_LINEIN);
  codec.volume(HEADPHONE_VOLUME);
  codec.lineOutLevel(13);                        // line out at maximum: 3.16 V p-p (default 29 = 1.29 V p-p)
  codec.lineInLevel(5);                          // line in sensitivity: 1.33 V p-p (default)

  g_drift = DRIFT;
  core = new EngineCore();
  core->init();
  g_sysexSender = onSysex;
  g_sysexUser = nullptr;

  flashOK = flashFS.begin(256 * 1024);
  sdOK = SD.begin(BUILTIN_SDCARD);
  bool loaded = loadMemories();

  myusb.begin();
  engineReady = true;

  Serial.println("Twenty Engine (Teensy 4.1)");
  Serial.printf("oscillators %d x 48 kHz, drift %.2f, audio block %d samples (%.1f ms)\n",
                MS20_OSF, DRIFT, AUDIO_BLOCK_SAMPLES, 1000.0f * AUDIO_BLOCK_SAMPLES / SAMPLE_RATE);
  Serial.printf("memory: flash %s, SD card %s, sounds %s\n", flashOK ? "OK" : "FAILED",
                sdOK ? "found (backup)" : "none", loaded ? "loaded" : "factory");
}

void loop() {
  myusb.Task();

  // iC -> engine + computer
  while (ic.read()) {
    uint8_t type = ic.getType();
    if (type == 0xF0) continue;                  // SysEx from the iC: not used
    fromIC(type, ic.getChannel(), ic.getData1(), ic.getData2());
  }

  // computer -> engine (notes, CC) and iC (SysEx, e.g. LEDs from the plugin)
  while (usbMIDI.read()) {
    uint8_t type = usbMIDI.getType();
    if (type == 0xF0) ic.sendSysEx(usbMIDI.getSysExArrayLength(), usbMIDI.getSysExArray(), true);
    else if (type >= 0x80 && type < 0xF0) queueMidi(type | ((usbMIDI.getChannel() - 1) & 0x0F), usbMIDI.getData1(), usbMIDI.getData2());
  }

  // engine -> iC LEDs
  while (ledTail != ledHead) {
    Led &m = ledQ[ledTail];
    ic.sendSysEx(m.len, m.b, true);
    ledTail = (ledTail + 1) & 63;
  }

  // save after a long press on a memory key
  if (core->o_Patch_Memory.savereq) {
    core->o_Patch_Memory.savereq = 0;
    if (saveMemories()) core->o_Patch_Memory.flash(31, 5);   // saved: all LEDs blink 5 times
    else core->o_Patch_Memory.flash(12, 8);                   // failed: PEAK+TRIG blink 8 times
  }

  // CPU load
  static elapsedMillis t;
  if (t > 2000) {
    t = 0;
    uint32_t c = cyclesMax; cyclesMax = 0;
    float budget = (float)F_CPU_ACTUAL * AUDIO_BLOCK_SAMPLES / SAMPLE_RATE;
    Serial.printf("engine CPU max %.1f %%  (audio blocks in use %d)\n", 100.0f * c / budget, AudioMemoryUsageMax());
  }
}
