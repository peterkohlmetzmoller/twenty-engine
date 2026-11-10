// led_port.h - sends the LED SysEx straight to the MS-20 iC.
// A plugin instrument in Logic cannot send MIDI to hardware itself, so the plugin opens the iC's
// MIDI destination through Core MIDI (macOS lets several programs use a port at the same time,
// so Logic keeps receiving the iC's knobs and keys).
// Windows: the same through WinMM. Some Windows MIDI drivers let only one program open a port; if the
// DAW holds the iC's output, opening fails and the LEDs simply stay as they are.
// Knobs in (Mac): the plugin also reads the knobs and patch cables straight from the controller, so the
// sound never depends on CCs stored in the DAW's track (performance recall, engine/recall.h). The track
// keeps the notes, the switch and the mod wheel. Where this is not possible (Windows, where a port can
// usually be opened by one program only) the knobs come through the track as before.
#pragma once
#include <stdint.h>
#include <atomic>

// knob messages from the Core MIDI thread to the audio thread (one producer, one consumer)
struct KnobQueue {
  uint32_t buf[2048]; std::atomic<int> head{0}, tail{0};
  void push(uint32_t v) { int h = head.load(std::memory_order_relaxed), n = (h + 1) & 2047; if (n == tail.load(std::memory_order_acquire)) return; buf[h] = v; head.store(n, std::memory_order_release); }
  bool pop(uint32_t &v) { int t = tail.load(std::memory_order_relaxed); if (t == head.load(std::memory_order_acquire)) return false; v = buf[t]; tail.store((t + 1) & 2047, std::memory_order_release); return true; }
};

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

class LedPort
{
public:
  LedPort()
  {
    if (MIDIClientCreate(CFSTR("TwentyEngine LEDs"), nullptr, nullptr, &mClient) == noErr)
    {
      MIDIOutputPortCreate(mClient, CFSTR("LED out"), &mPort);
      MIDIInputPortCreate(mClient, CFSTR("Knobs in"), &LedPort::ReadProc, this, &mIn);
    }
  }
  ~LedPort()
  {
    if (mIn && mSrc) MIDIPortDisconnectSource(mIn, mSrc);
    if (mIn) MIDIPortDispose(mIn);
    if (mPort) MIDIPortDispose(mPort);
    if (mClient) MIDIClientDispose(mClient);
  }
  // audio thread: next message from the controller (status, data1, data2)
  bool PopKnob(uint8_t &s, uint8_t &d1, uint8_t &d2) { uint32_t v; if (!mKnobs.pop(v)) return false; s = v >> 16; d1 = v >> 8; d2 = v; return true; }
  // any thread: the knobs reach the plugin straight from the controller
  bool KnobsDirect() const { return mDirect.load(std::memory_order_relaxed); }
  // look for a destination whose name contains "MS-20" (the iC shows up as "MS-20 Controller")
  void Rescan()
  {
    RescanInput();
    mDest = 0;
    if (!mPort) return;
    const ItemCount n = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < n; i++)
    {
      MIDIEndpointRef ep = MIDIGetDestination(i);
      CFStringRef name = nullptr;
      if (MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &name) == noErr && name)
      {
        const bool match = CFStringFind(name, CFSTR("MS-20"), 0).location != kCFNotFound;
        CFRelease(name);
        if (match) { mDest = ep; return; }
      }
    }
  }
  void Send(const uint8_t* bytes, int len)
  {
    if (!mDest || len <= 0 || len > 64) return;
    Byte buf[256];
    MIDIPacketList* pl = (MIDIPacketList*)buf;
    MIDIPacket* pk = MIDIPacketListInit(pl);
    pk = MIDIPacketListAdd(pl, sizeof(buf), pk, 0, (ByteCount)len, bytes);
    if (pk) MIDISend(mPort, mDest, pl);
  }
  bool Connected() const { return mDest != 0; }

  // Which controller is plugged in, judged from the MIDI inputs the Mac sees:
  // 0 none, 1 stock MS-20 iC, 2 Twenty Engine on Ksoloti, 3 Twenty Engine on Teensy.
  // The hardware versions win: with them inside, the iC itself is not on USB.
  static int FindController()
  {
    int found = 0;
    const ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; i++)
    {
      CFStringRef name = nullptr;
      if (MIDIObjectGetStringProperty(MIDIGetSource(i), kMIDIPropertyDisplayName, &name) != noErr || !name) continue;
      auto has = [&](CFStringRef s) { return CFStringFind(name, s, kCFCompareCaseInsensitive).location != kCFNotFound; };
      int k = 0;
      if (has(CFSTR("Ksoloti"))) k = 2;
      else if (has(CFSTR("Teensy")) || has(CFSTR("Twenty Engine"))) k = 3;
      else if (has(CFSTR("MS-20"))) k = 1;
      CFRelease(name);
      if (k > found) found = k;
    }
    return found;
  }

private:
  MIDIClientRef mClient = 0;
  MIDIPortRef mPort = 0, mIn = 0;
  MIDIEndpointRef mDest = 0, mSrc = 0;
  KnobQueue mKnobs;
  std::atomic<bool> mDirect{false};
  uint8_t mRun = 0, mD[2] = {0, 0}; int mNd = 0; bool mSysex = false;   // parser state (Core MIDI thread)

  // the controller's MIDI source, by the same names as FindController (the hardware versions first)
  void RescanInput()
  {
    if (!mIn) return;
    MIDIEndpointRef best = 0; int bestK = 0;
    const ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; i++)
    {
      MIDIEndpointRef ep = MIDIGetSource(i);
      CFStringRef name = nullptr;
      if (MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &name) != noErr || !name) continue;
      auto has = [&](CFStringRef t) { return CFStringFind(name, t, kCFCompareCaseInsensitive).location != kCFNotFound; };
      int k = 0;
      if (has(CFSTR("Ksoloti"))) k = 2;
      else if (has(CFSTR("Teensy")) || has(CFSTR("Twenty Engine"))) k = 3;
      else if (has(CFSTR("MS-20"))) k = 1;
      CFRelease(name);
      if (k > bestK) { bestK = k; best = ep; }
    }
    if (best == mSrc) { mDirect.store(best != 0); return; }
    if (mSrc) MIDIPortDisconnectSource(mIn, mSrc);
    mSrc = 0;
    if (best && MIDIPortConnectSource(mIn, best, nullptr) == noErr) mSrc = best;
    mDirect.store(mSrc != 0);
  }
  // Core MIDI thread: everything the controller plays (notes, knobs, cables, switch, wheel): it plays the
  // plugin's live engine directly, like the hardware (performance recall, engine/recall.h)
  static void ReadProc(const MIDIPacketList *pl, void *self, void *)
  {
    LedPort *p = (LedPort *)self;
    const MIDIPacket *pk = &pl->packet[0];
    for (UInt32 i = 0; i < pl->numPackets; i++)
    {
      for (UInt16 j = 0; j < pk->length; j++) p->Parse(pk->data[j]);
      pk = MIDIPacketNext(pk);
    }
  }
  void Parse(uint8_t b)
  {
    if (b >= 0xF8) return;                                   // realtime
    if (b == 0xF0) { mSysex = true; return; }
    if (b == 0xF7) { mSysex = false; return; }
    if (mSysex) return;
    if (b & 0x80) { mRun = (b < 0xF0) ? b : 0; mNd = 0; return; }
    if (!mRun) return;
    mD[mNd++] = b;
    const int need = ((mRun & 0xF0) == 0xC0 || (mRun & 0xF0) == 0xD0) ? 1 : 2;
    if (mNd < need) return;
    mNd = 0;
    mKnobs.push(((uint32_t)mRun << 16) | ((uint32_t)mD[0] << 8) | (need > 1 ? mD[1] : 0));
  }
};

#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <ctype.h>
#pragma comment(lib, "winmm.lib")

class LedPort
{
public:
  ~LedPort() { Close(); CloseIn(); }
  // look for an output whose name contains "MS-20"; keep it open while it works.
  // Also open the controller's input for the plugin itself: Windows 11 with the new Windows MIDI Services
  // lets several programs use one port (the DAW keeps it too). Older Windows: the DAW usually holds the
  // port, opening fails, and the plugin works through the track as before.
  void Rescan()
  {
    RescanInput();
    if (mOut) return;
    const UINT n = midiOutGetNumDevs();
    for (UINT i = 0; i < n; i++)
    {
      MIDIOUTCAPSA caps;
      if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
      if (!Has(caps.szPname, "ms-20")) continue;
      if (midiOutOpen(&mOut, i, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) mOut = nullptr;
      return;
    }
  }
  void Send(const uint8_t* bytes, int len)
  {
    if (!mOut || len <= 0 || len > 64) return;
    MMRESULT r;
    if (bytes[0] == 0xF0)
    {
      memcpy(mBuf, bytes, len);
      MIDIHDR h = {};
      h.lpData = (LPSTR)mBuf; h.dwBufferLength = (DWORD)len; h.dwBytesRecorded = (DWORD)len;
      r = midiOutPrepareHeader(mOut, &h, sizeof(h));
      if (r == MMSYSERR_NOERROR)
      {
        r = midiOutLongMsg(mOut, &h, sizeof(h));
        for (int k = 0; k < 50 && !(h.dwFlags & MHDR_DONE); k++) Sleep(1);   // a 9-byte LED message takes ~3 ms
        midiOutUnprepareHeader(mOut, &h, sizeof(h));
      }
    }
    else
    {
      DWORD m = bytes[0] | ((len > 1 ? bytes[1] : 0) << 8) | ((len > 2 ? bytes[2] : 0) << 16);
      r = midiOutShortMsg(mOut, m);
    }
    if (r != MMSYSERR_NOERROR) Close();   // unplugged: look again at the next rescan
  }
  bool Connected() const { return mOut != nullptr; }
  bool PopKnob(uint8_t &s, uint8_t &d1, uint8_t &d2) { uint32_t v; if (!mKnobs.pop(v)) return false; s = (uint8_t)(v >> 16); d1 = (uint8_t)(v >> 8); d2 = (uint8_t)v; return true; }
  bool KnobsDirect() const { return mIn != nullptr && mInOk.load(std::memory_order_relaxed); }

  // Which controller is plugged in (same rules as on the Mac), from the MIDI inputs Windows lists.
  static int FindController()
  {
    int found = 0;
    const UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; i++)
    {
      MIDIINCAPSA caps;
      if (midiInGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
      int k = 0;
      if (Has(caps.szPname, "ksoloti")) k = 2;
      else if (Has(caps.szPname, "teensy") || Has(caps.szPname, "twenty engine")) k = 3;
      else if (Has(caps.szPname, "ms-20")) k = 1;
      if (k > found) found = k;
    }
    return found;
  }

private:
  static bool Has(const char* name, const char* lowerNeedle)
  {
    char low[64]; int i = 0;
    for (; name[i] && i < 63; i++) low[i] = (char)tolower((unsigned char)name[i]);
    low[i] = 0;
    return strstr(low, lowerNeedle) != nullptr;
  }
  void Close() { if (mOut) { midiOutReset(mOut); midiOutClose(mOut); mOut = nullptr; } }
  HMIDIOUT mOut = nullptr;
  // ---- controller input (straight to the plugin) ----
  HMIDIIN mIn = nullptr; int mInDev = -1; KnobQueue mKnobs; std::atomic<bool> mInOk{false};
  void CloseIn() { if (mIn) { mInOk.store(false); midiInStop(mIn); midiInReset(mIn); midiInClose(mIn); mIn = nullptr; mInDev = -1; } }
  void RescanInput()
  {
    // the controller, by the same names as FindController (the hardware versions first)
    int best = -1, bestK = 0;
    const UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; i++)
    {
      MIDIINCAPSA caps;
      if (midiInGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
      int k = 0;
      if (Has(caps.szPname, "ksoloti")) k = 2;
      else if (Has(caps.szPname, "teensy") || Has(caps.szPname, "twenty engine")) k = 3;
      else if (Has(caps.szPname, "ms-20")) k = 1;
      if (k > bestK) { bestK = k; best = (int)i; }
    }
    if (best == mInDev && mIn) return;
    CloseIn();
    if (best < 0) return;
    if (midiInOpen(&mIn, (UINT)best, (DWORD_PTR)&LedPort::InProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) { mIn = nullptr; return; }
    if (midiInStart(mIn) != MMSYSERR_NOERROR) { midiInClose(mIn); mIn = nullptr; return; }
    mInDev = best; mInOk.store(true);
  }
  // WinMM's thread: channel messages arrive whole (dwParam1 = status | data1 << 8 | data2 << 16)
  static void CALLBACK InProc(HMIDIIN, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR)
  {
    if (msg != MIM_DATA) return;
    LedPort *self = (LedPort *)inst;
    const uint8_t s = (uint8_t)(p1 & 0xFF), d1 = (uint8_t)((p1 >> 8) & 0x7F), d2 = (uint8_t)((p1 >> 16) & 0x7F);
    if (s < 0x80 || s >= 0xF0) return;
    const bool one = (s & 0xF0) == 0xC0 || (s & 0xF0) == 0xD0;
    self->mKnobs.push(((uint32_t)s << 16) | ((uint32_t)d1 << 8) | (one ? 0 : d2));
  }
  char mBuf[64];
};

#else
class LedPort
{
public:
  void Rescan() {}
  void Send(const uint8_t*, int) {}
  bool Connected() const { return false; }
  bool PopKnob(uint8_t &, uint8_t &, uint8_t &) { return false; }
  bool KnobsDirect() const { return false; }
  static int FindController() { return 0; }
};
#endif
