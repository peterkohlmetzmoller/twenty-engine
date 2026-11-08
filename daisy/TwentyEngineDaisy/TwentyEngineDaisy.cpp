// Twenty Engine - Daisy Patch SM edition
// Copyright (C) Peter Kohlmetz Moller. GPL v3 (see LICENSE in the repository).
//
// The same engine as the Ksoloti patch, the plugin and the Teensy (engine/engine_gen.h, generated from the .axp),
// in the HQ form: oversampled oscillators and arp filter, triangle correction, analog drift.
//
//  - MS-20 iC on the USB HOST pins of the Patch SM: A8 = D-, A9 = D+ (the iC needs 5 V from outside, see the guide)
//  - USB device = the Patch SM's own USB-C socket: shows up on the computer as "Twenty Engine";
//    everything from the iC is passed through (memory keys as CC 102/103, silent in a DAW),
//    notes/CC from the computer play the engine, SysEx from the computer goes to the iC (LEDs)
//  - audio out: OUT_L / OUT_R, 48 kHz, 16-sample blocks (0.33 ms)
//  - audio in: IN_L -> the engine's external input (EXT SIGNAL)
//  - patch memory on the keys: saved in the Patch SM's QSPI flash (no SD card needed)
//  - the Patch SM's LED: slow blink = waiting for the iC, steady = iC connected, fast flicker = memory saved

// ================= SETTINGS (oversampling: MS20_OSF in the Makefile, 2 = 96 kHz, 1 = less CPU) =================

static const float DRIFT = 0.5f;         // analog drift 0.0 .. 1.0 (Teensy default)
static const float OUTPUT_LEVEL = 0.35f; // the Patch SM outputs are Eurorack level (about 10 Vpp at 1.0):
                                         // 0.35 is roughly line level for the iC's jacks / a mixer
// ============================================

// MS20_RAMFILE and MS20_OSF are set in the Makefile (every engine file must see them)
#include "daisy_patch_sm.h"
#include "usbh_midi.h"
#include "engine_gen.h"

using namespace daisy;
using namespace patch_sm;
using namespace ms20;

extern "C" { extern USBH_HandleTypeDef hUsbHostHS; }   // libDaisy's USB host handle (USB HS = pins A8/A9)

DaisyPatchSM   hw;
USBHostHandle  usbHost;
MidiUsbHandler usbDevice;                // to the computer (Patch SM USB-C, USB FS)
static EngineCore core __attribute__((section(".axi_bss")));   // 274 KB, in the fast AXI SRAM (see the .lds)

static volatile bool icActive = false;

// ---------------- MIDI from the iC / computer to the audio interrupt ----------------
struct Msg { uint8_t s, d1, d2; };
static Msg midiQ[256];
static volatile uint8_t midiHead = 0, midiTail = 0;
static void queueMidi(uint8_t s, uint8_t d1, uint8_t d2) {
    uint8_t n = midiHead + 1;
    if(n == midiTail) return;           // full: drop
    midiQ[midiHead] = {s, d1, d2};
    midiHead = n;
}

// ---------------- LED SysEx from the engine (audio interrupt) to the iC (main loop) ----------------
struct LedMsg { uint8_t b[16]; uint8_t len; };
static LedMsg ledQ[64];
static volatile uint8_t ledHead = 0, ledTail = 0;
static void onSysex(const uint8_t *b, int len, void *) {
    uint8_t n = (ledHead + 1) & 63;
    if(n == ledTail) return;
    LedMsg &m = ledQ[ledHead];
    m.len = len > 16 ? 16 : len;
    memcpy(m.b, b, m.len);
    ledHead = n;
}

// SysEx -> USB-MIDI packets (CIN 4 = start/continue, 5/6/7 = end with 1/2/3 bytes), sent to the iC
static uint8_t DMA_BUFFER_MEM_SECTION hostTx[64];
static void sendSysexToIC(const uint8_t *b, int len) {
    if(!icActive) return;
    int n = 0, i = 0;
    while(i < len && n + 4 <= (int)sizeof(hostTx)) {
        int left = len - i;
        if(left > 3) { hostTx[n] = 0x04; memcpy(hostTx + n + 1, b + i, 3); i += 3; }
        else { hostTx[n] = (uint8_t)(0x04 + left); memset(hostTx + n + 1, 0, 3); memcpy(hostTx + n + 1, b + i, left); i += left; }
        n += 4;
    }
    for(int tries = 0; tries < 20; tries++) {        // the host pipe can be busy for a moment
        if(USBH_MIDI_Transmit(&hUsbHostHS, hostTx, n) != MIDI_BUSY) break;
        System::DelayUs(100);
    }
}

// ---------------- MIDI thru: iC -> engine + computer ----------------
static bool memArmed = false;            // momentary switch held = keys pick memory slots

static void toComputer(uint8_t s, uint8_t d1, uint8_t d2) {
    uint8_t b[3] = {s, d1, d2};
    uint8_t h = s & 0xF0;
    usbDevice.SendMessage(b, (h == 0xC0 || h == 0xD0) ? 2 : 3);
}

static void fromIC(uint8_t s, uint8_t d1, uint8_t d2) {
    queueMidi(s, d1, d2);                                  // to the engine
    uint8_t h = s & 0xF0, ch = s & 0x0F;
    if(h == 0xD0) memArmed = (d1 > 0) && !core.o_Patch_Memory.tapmode;
    // to the computer: memory keys become CC 102 (down) / 103 (up) so a DAW doesn't play them
    if(memArmed && h == 0x90 && d2 > 0) toComputer(0xB0 | ch, 102, d1);
    else if(memArmed && (h == 0x80 || h == 0x90)) toComputer(0xB0 | ch, 103, d1);
    else toComputer(s, d1, d2);
}

// USB host receive (called from usbHost.Process() in the main loop): 4-byte USB-MIDI packets
static void hostRx(uint8_t *buf, size_t len, void *) {
    for(size_t i = 0; i + 3 < len; i += 4) {
        uint8_t cin = buf[i] & 0x0F;
        if(cin >= 0x08 && cin <= 0x0E) fromIC(buf[i + 1], buf[i + 2], buf[i + 3]);   // channel messages
        // SysEx from the iC (cin 4..7) is not used
    }
}

static void onClassActive(void *) {
    if(usbHost.IsActiveClass(USBH_MIDI_CLASS)) {
        USBH_MIDI_SetReceiveCallback(&hUsbHostHS, hostRx, nullptr);
        icActive = true;
    }
}
static void onDisconnect(void *) { icActive = false; }

// ---------------- patch memory in the QSPI flash ----------------
static const uint32_t MEM_OFFSET = 0x700000;      // 7 MB into the 8 MB flash: clear of QSPI-loaded apps
static const uint32_t MEM_MAGIC = 0x30325754;     // "TW20"
static uint8_t DMA_BUFFER_MEM_SECTION qspiBuf[sizeof(g_ramFile) + 8];

static void loadMemories() {
    const uint8_t *p = (const uint8_t *)hw.qspi.GetData(MEM_OFFSET);
    uint32_t magic, len;
    memcpy(&magic, p, 4); memcpy(&len, p + 4, 4);
    if(magic == MEM_MAGIC && len > 0 && len <= sizeof(g_ramFile)) {
        memcpy(g_ramFile, p + 8, len);
        g_ramFileLen = len;
        core.o_Patch_Memory.loadFile();
    }                                               // otherwise: factory sounds
}

static bool saveMemories() {
    uint32_t len = g_ramFileLen;
    memcpy(qspiBuf, &MEM_MAGIC, 4); memcpy(qspiBuf + 4, &len, 4); memcpy(qspiBuf + 8, g_ramFile, len);
    if(hw.qspi.Erase(MEM_OFFSET, MEM_OFFSET + 0x10000) != QSPIHandle::Result::OK) return false;
    if(hw.qspi.Write(MEM_OFFSET, len + 8, qspiBuf) != QSPIHandle::Result::OK) return false;
    dsy_dma_invalidate_cache_for_buffer((uint8_t *)hw.qspi.GetData(MEM_OFFSET), len + 8);
    const uint8_t *p = (const uint8_t *)hw.qspi.GetData(MEM_OFFSET);
    return memcmp(p, qspiBuf, len + 8) == 0;        // read back
}

// ---------------- audio ----------------
static volatile uint32_t cyclesMax = 0;

static void audioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    uint32_t t0 = DWT->CYCCNT;
    while(midiTail != midiHead) {                  // MIDI first, like the Ksoloti does each block
        Msg m = midiQ[midiTail];
        core.midi(m.s, m.d1, m.d2);
        midiTail = midiTail + 1;
    }
    const float toEngine = 134217728.f, fromEngine = OUTPUT_LEVEL / 134217728.f;
    for(size_t b = 0; b < size; b += BUFSIZE) {
        for(int i = 0; i < BUFSIZE; i++) {
            float x = IN_L[b + i] * toEngine;
            core.audioInL[i] = (int32_t)(x > 134217727.f ? 134217727.f : (x < -134217728.f ? -134217728.f : x));
        }
        core.process();
        for(int i = 0; i < BUFSIZE; i++) {
            OUT_L[b + i] = core.audioOutL[i] * fromEngine;
            OUT_R[b + i] = core.audioOutR[i] * fromEngine;
        }
    }
    uint32_t c = DWT->CYCCNT - t0;
    if(c > cyclesMax) cyclesMax = c;
}

int main(void) {
    hw.Init();
    hw.StartLog(false);                            // serial log over the USB-C port is not used (MIDI owns it)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // cycle counter for the CPU measurement
    DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    g_drift = DRIFT;
    memset(&core, 0, sizeof(core));                // its section is not zeroed at start-up
    core.init();
    g_sysexSender = onSysex;
    g_sysexUser = nullptr;
    loadMemories();

    // computer side: class-compliant USB MIDI on the Patch SM's USB-C socket, named "Twenty Engine"
    MidiUsbHandler::Config dcfg;
    dcfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    usbDevice.Init(dcfg);
    usbDevice.StartReceive();

    // iC side: USB host on pins A8 / A9
    USBHostHandle::Config hcfg;
    hcfg.class_active_callback = onClassActive;
    hcfg.disconnect_callback = onDisconnect;
    usbHost.Init(hcfg);
    usbHost.RegisterClass(USBH_MIDI_CLASS);

    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(BUFSIZE);                 // 16 samples = 0.33 ms, the engine's own block
    hw.StartAudio(audioCallback);

    uint32_t ledFlash = 0;
    while(1) {
        usbHost.Process();

        // computer -> engine (notes, CC) and iC (SysEx, e.g. LEDs from the plugin)
        usbDevice.Listen();
        while(usbDevice.HasEvents()) {
            MidiEvent e = usbDevice.PopEvent();
            uint8_t ch = (uint8_t)(e.channel & 0x0F);
            switch(e.type) {
                case NoteOff: queueMidi(0x80 | ch, e.data[0], e.data[1]); break;
                case NoteOn: queueMidi(0x90 | ch, e.data[0], e.data[1]); break;
                case PolyphonicKeyPressure: queueMidi(0xA0 | ch, e.data[0], e.data[1]); break;
                case ControlChange:
                case ChannelMode: queueMidi(0xB0 | ch, e.data[0], e.data[1]); break;
                case ProgramChange: queueMidi(0xC0 | ch, e.data[0], 0); break;
                case ChannelPressure: queueMidi(0xD0 | ch, e.data[0], 0); break;
                case PitchBend: queueMidi(0xE0 | ch, e.data[0], e.data[1]); break;
                case SystemCommon:
                    if(e.sc_type == SystemExclusive) {      // libDaisy strips F0 / F7: put them back
                        uint8_t sx[SYSEX_BUFFER_LEN + 2]; int n = 0;
                        sx[n++] = 0xF0;
                        for(int i = 0; i < e.sysex_message_len; i++) sx[n++] = e.sysex_data[i];
                        sx[n++] = 0xF7;
                        sendSysexToIC(sx, n);
                    }
                    break;
                default: break;
            }
        }

        // engine -> iC LEDs
        while(ledTail != ledHead) {
            LedMsg &m = ledQ[ledTail];
            sendSysexToIC(m.b, m.len);
            ledTail = (ledTail + 1) & 63;
        }

        // save after a long press on a memory key
        if(core.o_Patch_Memory.savereq) {
            core.o_Patch_Memory.savereq = 0;
            core.o_Patch_Memory.saveFile();               // fills the RAM file (g_ramFileDirty)
        }
        if(g_ramFileDirty) {
            g_ramFileDirty = false;
            bool ok = saveMemories();
            if(ok) core.o_Patch_Memory.flash(31, 5);       // saved: all LEDs blink 5 times
            else core.o_Patch_Memory.flash(12, 8);         // failed: PEAK+TRIG blink 8 times
            if(ok) ledFlash = System::GetNow() + 600;
        }

        uint32_t now = System::GetNow();

        // CPU load to the computer every 2 s, as SysEx F0 7D 54 45 <percent> F7 (7D = non-commercial ID,
        // "TE"): visible in MIDI Monitor, ignored by DAWs. Percent of one 16-sample block at 48 kHz.
        static uint32_t cpuTime = 0;
        if(now - cpuTime > 2000) {
            cpuTime = now;
            uint32_t c = cyclesMax; cyclesMax = 0;
            float budget = (float)System::GetSysClkFreq() * BUFSIZE / 48000.f;
            int pct = (int)(100.f * c / budget + 0.5f); if(pct > 127) pct = 127;
            uint8_t sx[6] = {0xF0, 0x7D, 0x54, 0x45, (uint8_t)pct, 0xF7};
            usbDevice.SendMessage(sx, 6);
        }

        // the Patch SM's LED
        if(now < ledFlash) hw.SetLed((now / 40) & 1);
        else if(icActive) hw.SetLed(true);
        else hw.SetLed((now / 500) & 1);
    }
}
