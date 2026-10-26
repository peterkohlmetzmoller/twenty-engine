// led_port.h - sends the LED SysEx straight to the MS-20 iC.
// A plugin instrument in Logic cannot send MIDI to hardware itself, so the plugin opens the iC's
// MIDI destination through Core MIDI (macOS lets several programs use a port at the same time,
// so Logic keeps receiving the iC's knobs and keys). On other platforms it does nothing.
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

private:
  MIDIClientRef mClient = 0;
  MIDIPortRef mPort = 0;
  MIDIEndpointRef mDest = 0;
};

#else
class LedPort
{
public:
  void Rescan() {}
  void Send(const uint8_t*, int) {}
  bool Connected() const { return false; }
};
#endif
