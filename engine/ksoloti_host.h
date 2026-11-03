// ksoloti_host.h - stand-ins for the Ksoloti firmware functions the MS-20 engine uses,
// so the code written for the Ksoloti patch compiles and runs unchanged on a computer.
// Conventions kept from Ksoloti: frac32 full scale = 1<<27, pitch 0 = MIDI note 64,
// 2^21 per semitone, control rate = sample rate / 16 (16-sample blocks).
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846   /* not in MSVC's math.h by default */
#endif
#include <stdio.h>
#include <new>
#if defined(TEENSYDUINO)
#include <SD.h>
#define MS20_TLS                     /* Teensy: bare metal, one audio thread - no thread-local storage */
#else
#define MS20_TLS thread_local
#endif

namespace ms20 {

#ifndef BUFSIZE
#define BUFSIZE 16
#endif
typedef int32_t int32buffer[BUFSIZE];

#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON 0x90
#define MIDI_CONTROL_CHANGE 0xB0

enum midi_device_t { MIDI_DEVICE_DIN = 0, MIDI_DEVICE_USB_DEVICE, MIDI_DEVICE_USB_HOST, MIDI_DEVICE_INTERNAL };

#if defined(__SSAT) && defined(__USAT)
// Teensy (CMSIS, like the Ksoloti): __SSAT/__USAT are the ARM saturate instructions; the bit count must be a constant
#else
static inline int32_t __SSAT(int32_t x, int b) { int32_t m = (1 << (b - 1)); if (x > m - 1) return m - 1; if (x < -m) return -m; return x; }
static inline int32_t __USAT(int32_t x, int b) { int32_t m = (1 << b) - 1; if (x > m) return m; if (x < 0) return 0; return x; }
#endif

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

extern MS20_TLS uint32_t g_ks_rng;   // firmware random generator state (reset by EngineCore::init)
static inline uint32_t GenerateRandomNumber() { g_ks_rng = g_ks_rng * 1664525u + 1013904223u; return g_ks_rng; }

// Parameter storage (the Ksoloti "PExch" table). The host reads and writes .value.
struct ParameterExchange_t { int32_t value; int32_t signals; };

// Changes made from inside the engine (CC assignments, ESP knob layers, tap tempo, patch bay
// selectors) just write the value; a host compares snapshots to update its own display.
static inline void PExParameterChange(ParameterExchange_t *p, int32_t v, uint32_t) { p->value = v; }

// MIDI out to the controller (LED SysEx). The host installs the sender.
typedef void (*SysexSender)(const uint8_t *bytes, int len, void *user);
extern MS20_TLS SysexSender g_sysexSender;
extern MS20_TLS void *g_sysexUser;
static inline void MidiSendSysEx(midi_device_t, uint8_t, uint8_t bytes[], uint8_t len) { if (g_sysexSender) g_sysexSender(bytes, len, g_sysexUser); }
static inline int MidiGetOutputBufferAvailable(midi_device_t) { return 63; }
static inline void LogTextMessage(const char *, ...) {}


// ---- SD card (FatFS) stand-ins: the patch memory file MS20MEM.BIN ----
typedef unsigned int UINT;
typedef int FRESULT;
enum { FR_OK = 0, FR_DISK_ERR = 1, FR_NO_FILE = 4 };
enum { FA_READ = 0x01, FA_OPEN_EXISTING = 0x00, FA_WRITE = 0x02, FA_CREATE_ALWAYS = 0x08 };
inline uint32_t fbuff[256];                       // the firmware's general-purpose SD buffer
#if defined(TEENSYDUINO)
// Teensy 4.1: the built-in SD slot; the sketch calls SD.begin(BUILTIN_SDCARD) and sets g_sdOK
inline bool g_sdOK = false;
struct FIL { File f; };
static inline FRESULT f_open(FIL *o, const char *name, int mode) {
    if (!g_sdOK) return FR_DISK_ERR;
    if (mode & FA_WRITE) { SD.remove(name); o->f = SD.open(name, FILE_WRITE_BEGIN); }
    else o->f = SD.open(name, FILE_READ);
    return o->f ? FR_OK : FR_NO_FILE; }
static inline FRESULT f_read(FIL *o, void *b, UINT n, UINT *br) { int r = o->f.read(b, n); *br = r > 0 ? (UINT)r : 0; return FR_OK; }
static inline FRESULT f_write(FIL *o, const void *b, UINT n, UINT *bw) { *bw = (UINT)o->f.write((const uint8_t *)b, n); return FR_OK; }
static inline FRESULT f_close(FIL *o) { o->f.close(); return FR_OK; }
#else
// computer: the patch writes "/MS20MEM.BIN"; that becomes <memory folder>/MS20MEM.BIN
struct FIL { FILE *f; };
inline char g_memDir[1024] = "";                  // set by the host (main thread) before loading
static inline FRESULT f_open(FIL *o, const char *name, int mode) {
    char path[1200]; snprintf(path, sizeof(path), "%s%s", g_memDir, (name[0] == '/') ? name + 1 : name);
    o->f = fopen(path, (mode & FA_WRITE) ? "wb" : "rb"); return o->f ? FR_OK : FR_NO_FILE; }
static inline FRESULT f_read(FIL *o, void *b, UINT n, UINT *br) { *br = (UINT)fread(b, 1, n, o->f); return FR_OK; }
static inline FRESULT f_write(FIL *o, const void *b, UINT n, UINT *bw) { *bw = (UINT)fwrite(b, 1, n, o->f); return FR_OK; }
static inline FRESULT f_close(FIL *o) { if (o->f) fclose(o->f); o->f = nullptr; return FR_OK; }
#endif

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
