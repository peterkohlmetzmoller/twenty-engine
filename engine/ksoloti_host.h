// ksoloti_host.h - stand-ins for the Ksoloti firmware functions the MS-20 engine uses,
// so the code written for the Ksoloti patch compiles and runs unchanged on a computer.
// Conventions kept from Ksoloti: frac32 full scale = 1<<27, pitch 0 = MIDI note 64,
// 2^21 per semitone, control rate = sample rate / 16 (16-sample blocks).
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <new>

namespace ms20 {

#ifndef BUFSIZE
#define BUFSIZE 16
#endif
typedef int32_t int32buffer[BUFSIZE];

#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON 0x90
#define MIDI_CONTROL_CHANGE 0xB0

enum midi_device_t { MIDI_DEVICE_DIN = 0, MIDI_DEVICE_USB_DEVICE, MIDI_DEVICE_USB_HOST, MIDI_DEVICE_INTERNAL };

static inline int32_t __SSAT(int32_t x, int b) { int32_t m = (1 << (b - 1)); if (x > m - 1) return m - 1; if (x < -m) return -m; return x; }
static inline int32_t __USAT(int32_t x, int b) { int32_t m = (1 << b) - 1; if (x > m) return m; if (x < 0) return 0; return x; }

// Ksoloti's mtof for 48 kHz: pitch (2^21 per semitone, 0 = MIDI 64) -> phase increment f/48000 * 2^32.
// Built from a 260-entry semitone table with linear interpolation, like the firmware's lookup.
struct MtofTable {
    double t[260];
    MtofTable() {
        for (int i = 0; i < 260; i++) {
            double f = 440.0 * pow(2.0, ((i - 128) + 64.0 - 69.0) / 12.0) / 48000.0 * 4294967296.0;
            t[i] = f > 4294967295.0 ? 4294967295.0 : f;
        }
    }
};
static inline uint32_t mtof_host(int32_t p) {
    static MtofTable tab;
    p = __SSAT(p, 29);
    int32_t pi = p >> 21;
    double fr = (p & 0x1FFFFF) / 2097152.0;
    double v = tab.t[128 + pi] + fr * (tab.t[129 + pi] - tab.t[128 + pi]);
    return (uint32_t)v;
}
#define MTOFEXTENDED(pitch, frequency) frequency = ms20::mtof_host(pitch);

extern thread_local uint32_t g_ks_rng;   // firmware random generator state (reset by EngineCore::init)
static inline uint32_t GenerateRandomNumber() { g_ks_rng = g_ks_rng * 1664525u + 1013904223u; return g_ks_rng; }

// Parameter storage (the Ksoloti "PExch" table). The host reads and writes .value.
struct ParameterExchange_t { int32_t value; int32_t signals; };

// Changes made from inside the engine (CC assignments, ESP knob layers, tap tempo, patch bay
// selectors) just write the value; a host compares snapshots to update its own display.
static inline void PExParameterChange(ParameterExchange_t *p, int32_t v, uint32_t) { p->value = v; }

// MIDI out to the controller (LED SysEx). The host installs the sender.
typedef void (*SysexSender)(const uint8_t *bytes, int len, void *user);
extern thread_local SysexSender g_sysexSender;
extern thread_local void *g_sysexUser;
static inline void MidiSendSysEx(midi_device_t, uint8_t, uint8_t bytes[], uint8_t len) { if (g_sysexSender) g_sysexSender(bytes, len, g_sysexUser); }
static inline int MidiGetOutputBufferAvailable(midi_device_t) { return 63; }
static inline void LogTextMessage(const char *, ...) {}


// ---- SD card (FatFS) stand-ins: the patch memory file lives in a folder the host chooses ----
// The patch writes "/MS20MEM.BIN"; on a computer that becomes <memory folder>/MS20MEM.BIN.
typedef unsigned int UINT;
typedef int FRESULT;
struct FIL { FILE *f; };
enum { FR_OK = 0, FR_NO_FILE = 4 };
enum { FA_READ = 0x01, FA_OPEN_EXISTING = 0x00, FA_WRITE = 0x02, FA_CREATE_ALWAYS = 0x08 };
inline char g_memDir[1024] = "";                  // set by the host (main thread) before loading
inline uint32_t fbuff[256];                       // the firmware's general-purpose SD buffer
static inline FRESULT f_open(FIL *o, const char *name, int mode) {
    char path[1200]; snprintf(path, sizeof(path), "%s%s", g_memDir, (name[0] == '/') ? name + 1 : name);
    o->f = fopen(path, (mode & FA_WRITE) ? "wb" : "rb"); return o->f ? FR_OK : FR_NO_FILE; }
static inline FRESULT f_read(FIL *o, void *b, UINT n, UINT *br) { *br = (UINT)fread(b, 1, n, o->f); return FR_OK; }
static inline FRESULT f_write(FIL *o, const void *b, UINT n, UINT *bw) { *bw = (UINT)fwrite(b, 1, n, o->f); return FR_OK; }
static inline FRESULT f_close(FIL *o) { if (o->f) fclose(o->f); o->f = nullptr; return FR_OK; }

// ---- ChibiOS thread stand-ins: the plugin has no background thread in the engine.
// The host loads the memory file once and saves it from its own main thread when the patch asks (savereq).
typedef int msg_t;
struct Thread {};
typedef msg_t (*tfunc_t)(void *);
#define WORKING_AREA(name, size) char name[1]
#define LOWPRIO 1
static inline Thread *chThdCreateStatic(void *, int, int, tfunc_t, void *) { return nullptr; }
static inline bool chThdShouldTerminate() { return true; }
static inline void chThdExit(msg_t) {}
static inline void chThdSleepMilliseconds(int) {}
static inline void chThdTerminate(Thread *) {}
static inline void chThdWait(Thread *) {}

} // namespace ms20
