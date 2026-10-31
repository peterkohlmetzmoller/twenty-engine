// led_port.h - sends the LED SysEx straight to the MS-20 iC.
// A plugin instrument in Logic cannot send MIDI to hardware itself, so the plugin opens the iC's
// MIDI destination through Core MIDI (macOS lets several programs use a port at the same time,
// so Logic keeps receiving the iC's knobs and keys).
// Windows: the same through WinMM. Some Windows MIDI drivers let only one program open a port; if the
// DAW holds the iC's output, opening fails and the LEDs simply stay as they are.
#pragma once
#include <stdint.h>

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

class LedPort
{
public:
  LedPort()
  {
    if (MIDIClientCreate(CFSTR("TwentyEngine LEDs"), nullptr, nullptr, &mClient) == noErr)
      MIDIOutputPortCreate(mClient, CFSTR("LED out"), &mPort);
  }
  ~LedPort()
  {
    if (mPort) MIDIPortDispose(mPort);
    if (mClient) MIDIClientDispose(mClient);
  }
  // look for a destination whose name contains "MS-20" (the iC shows up as "MS-20 Controller")
  void Rescan()
  {
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
  MIDIPortRef mPort = 0;
  MIDIEndpointRef mDest = 0;
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
  ~LedPort() { Close(); }
  // look for an output whose name contains "MS-20"; keep it open while it works
  void Rescan()
  {
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
  char mBuf[64];
};

#else
class LedPort
{
public:
  void Rescan() {}
  void Send(const uint8_t*, int) {}
  bool Connected() const { return false; }
  static int FindController() { return 0; }
};
#endif
