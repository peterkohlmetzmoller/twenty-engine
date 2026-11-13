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

// Controllers, by the names their MIDI ports show: 0 none, 1 stock MS-20ic, 2 Twenty Engine on Ksoloti,
// 3 Twenty Engine on Teensy, 4 Korg microKONTROL (engine/mk_control.h). The MS-20 family always wins (with the
// hardware versions the iC itself is not on USB); the microKONTROL is used only when none of them is there.
enum { CTRL_NONE = 0, CTRL_IC = 1, CTRL_KSOLOTI = 2, CTRL_TEENSY = 3, CTRL_MK = 4 };
static inline int CtrlRank(int k) { static const int r[] = {0, 2, 3, 4, 1}; return (k >= 0 && k <= 4) ? r[k] : 0; }
// the microKONTROL's native messages (F0 42 4g 6E 00 cmd a b F7), packed g << 24 | cmd << 16 | a << 8 | b
static inline bool MkNativeMsg(const uint8_t *b, int n, uint32_t &packed)
{
  if (n != 9 || b[0] != 0xF0 || b[1] != 0x42 || (b[2] & 0xF0) != 0x40 || b[3] != 0x6E || b[4] != 0x00 || b[8] != 0xF7) return false;
  packed = ((uint32_t)(b[2] & 15) << 24) | ((uint32_t)b[5] << 16) | ((uint32_t)b[6] << 8) | b[7];
  return true;
}

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
    MkNativeOff();
    for (int i = 0; i < mNSrc; i++) if (mIn) MIDIPortDisconnectSource(mIn, mSrc[i]);
    if (mIn) MIDIPortDispose(mIn);
    if (mPort) MIDIPortDispose(mPort);
    if (mClient) MIDIClientDispose(mClient);
  }
  // audio thread: next message from the controller (status, data1, data2)
  bool PopKnob(uint8_t &s, uint8_t &d1, uint8_t &d2) { uint32_t v; if (!mKnobs.pop(v)) return false; s = v >> 16; d1 = v >> 8; d2 = v; return true; }
  // audio thread: next native message from the microKONTROL (g = its global channel)
  bool PopNative(uint8_t &g, uint8_t &cmd, uint8_t &a, uint8_t &b) { uint32_t v; if (!mNative.pop(v)) return false; g = v >> 24; cmd = v >> 16; a = v >> 8; b = v; return true; }
  // any thread: the knobs reach the plugin straight from the controller
  bool KnobsDirect() const { return mDirect.load(std::memory_order_relaxed); }
  // any thread: the controller connected (CTRL_*), and for the microKONTROL what it sent besides native messages
  int Kind() const { return mKind.load(std::memory_order_relaxed); }
  bool MkActive() const { return Kind() == CTRL_MK && mHasDest.load(std::memory_order_relaxed); }
  int MkPlainCC() const { return mMkCC.load(std::memory_order_relaxed); }     // CCs (not native: native mode is off)
  bool MkBend() const { return mMkBend.load(std::memory_order_relaxed); }     // sends the joystick as pitch bend itself
  bool MkMod() const { return mMkMod.load(std::memory_order_relaxed); }       // ... and as CC 1
  // the controller's destination: the MS-20 family by "MS-20" (the iC shows up as "MS-20 Controller"),
  // the microKONTROL by its CTRL port (native mode and the displays)
  void Rescan()
  {
    RescanInput();
    if (!mPort) { mDest = 0; return; }
    const bool mk = Kind() == CTRL_MK;
    MIDIEndpointRef best = 0; int bestScore = 0;
    const ItemCount n = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < n; i++)
    {
      MIDIEndpointRef ep = MIDIGetDestination(i);
      CFStringRef name = nullptr;
      if (MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &name) != noErr || !name) continue;
      auto has = [&](CFStringRef t) { return CFStringFind(name, t, kCFCompareCaseInsensitive).location != kCFNotFound; };
      int score = 0;
      if (!mk) score = CFStringFind(name, CFSTR("MS-20"), 0).location != kCFNotFound ? 1 : 0;
      else if (has(CFSTR("microKONTROL"))) score = has(CFSTR("CTRL")) ? 3 : has(CFSTR("MIDI OUT")) ? 1 : 2;
      CFRelease(name);
      if (score > bestScore) { bestScore = score; best = ep; }
    }
    if (mk && best != mDest) mMkCC.store(0);
    mDest = best; mHasDest.store(best != 0);
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

  static int KindOf(CFStringRef name)
  {
    auto has = [&](CFStringRef s) { return CFStringFind(name, s, kCFCompareCaseInsensitive).location != kCFNotFound; };
    if (has(CFSTR("Twenty Engine Panel"))) return CTRL_NONE;   // our own port (PanelPort), never the controller
    if (has(CFSTR("Ksoloti"))) return CTRL_KSOLOTI;
    if (has(CFSTR("Teensy")) || has(CFSTR("Twenty Engine"))) return CTRL_TEENSY;
    if (has(CFSTR("MS-20"))) return CTRL_IC;
    if (has(CFSTR("microKONTROL"))) return CTRL_MK;
    return CTRL_NONE;
  }
  // Which controller is plugged in, judged from the MIDI inputs the Mac sees (CTRL_*).
  static int FindController()
  {
    int found = 0;
    const ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; i++)
    {
      CFStringRef name = nullptr;
      if (MIDIObjectGetStringProperty(MIDIGetSource(i), kMIDIPropertyDisplayName, &name) != noErr || !name) continue;
      const int k = KindOf(name);
      CFRelease(name);
      if (CtrlRank(k) > CtrlRank(found)) found = k;
    }
    return found;
  }

private:
  static const int MAXSRC = 4;
  MIDIClientRef mClient = 0;
  MIDIPortRef mPort = 0, mIn = 0;
  MIDIEndpointRef mDest = 0, mSrc[MAXSRC] = {};
  int mNSrc = 0;
  KnobQueue mKnobs, mNative;
  std::atomic<bool> mDirect{false}, mMkBend{false}, mMkMod{false}, mHasDest{false};
  std::atomic<int> mKind{0}, mMkCC{0}, mNoteSrc{-1}, mMkG{-1};
  // parser state per source (Core MIDI thread)
  struct Parser { uint8_t run = 0, d[2] = {0, 0}; int nd = 0; bool sysex = false; uint8_t sx[16]; int nsx = 0; };
  Parser mParse[MAXSRC];

  void MkNativeOff()
  {
    // leave native mode: the microKONTROL works as a normal MIDI keyboard again
    const int g = mMkG.load();
    if (Kind() != CTRL_MK || !mDest || g < 0) return;
    const uint8_t m[9] = {0xF0, 0x42, (uint8_t)(0x40 | g), 0x6E, 0x00, 0x00, 0x00, 0x00, 0xF7};
    Send(m, 9);
  }
  // the controller's MIDI source(s), by the same names as FindController: one source for the MS-20 family; for
  // the microKONTROL all of its sources (native messages and keys may come on different ports)
  void RescanInput()
  {
    if (!mIn) return;
    MIDIEndpointRef want[MAXSRC] = {}; int nWant = 0, bestK = 0;
    const ItemCount n = MIDIGetNumberOfSources();
    for (int pass = 0; pass < 2; pass++)
      for (ItemCount i = 0; i < n; i++)
      {
        MIDIEndpointRef ep = MIDIGetSource(i);
        CFStringRef name = nullptr;
        if (MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &name) != noErr || !name) continue;
        const int k = KindOf(name);
        CFRelease(name);
        if (pass == 0) { if (CtrlRank(k) > CtrlRank(bestK)) { bestK = k; want[0] = ep; nWant = k ? 1 : 0; } }
        else if (bestK == CTRL_MK && k == CTRL_MK && ep != want[0] && nWant < MAXSRC) want[nWant++] = ep;
      }
    bool same = nWant == mNSrc;
    for (int i = 0; same && i < nWant; i++) same = want[i] == mSrc[i];
    if (same) { mKind.store(bestK); mDirect.store(mNSrc > 0); return; }
    mDirect.store(false);
    for (int i = 0; i < mNSrc; i++) MIDIPortDisconnectSource(mIn, mSrc[i]);
    mNSrc = 0;
    if (bestK != Kind()) { mMkCC.store(0); mMkBend.store(false); mMkMod.store(false); mNoteSrc.store(-1); mMkG.store(-1); }
    mKind.store(bestK);
    for (int i = 0; i < nWant; i++)
    {
      mParse[mNSrc] = Parser();
      if (MIDIPortConnectSource(mIn, want[i], (void *)(intptr_t)mNSrc) == noErr) mSrc[mNSrc++] = want[i];
    }
    mDirect.store(mNSrc > 0);
  }
  // Core MIDI thread: everything the controller plays (notes, knobs, cables, switch, wheel): it plays the
  // plugin's live engine directly, like the hardware (performance recall, engine/recall.h)
  static void ReadProc(const MIDIPacketList *pl, void *self, void *src)
  {
    LedPort *p = (LedPort *)self;
    const int si = (int)(intptr_t)src;
    if (si < 0 || si >= MAXSRC) return;
    const MIDIPacket *pk = &pl->packet[0];
    for (UInt32 i = 0; i < pl->numPackets; i++)
    {
      for (UInt16 j = 0; j < pk->length; j++) p->Parse(pk->data[j], si);
      pk = MIDIPacketNext(pk);
    }
  }
  void Parse(uint8_t b, int si)
  {
    Parser &q = mParse[si];
    if (b >= 0xF8) return;                                   // realtime
    if (b == 0xF0) { q.sysex = true; q.nsx = 0; q.sx[q.nsx++] = b; return; }
    if (q.sysex)
    {
      if (q.nsx < (int)sizeof(q.sx)) q.sx[q.nsx] = b;
      q.nsx++;
      if (b == 0xF7)
      {
        q.sysex = false;
        uint32_t v;
        if (Kind() == CTRL_MK && MkNativeMsg(q.sx, q.nsx, v))
        {
          mMkG.store((int)(v >> 24)); mNative.push(v);
          if (((v >> 16) & 0xFF) == 0x40) mNoteSrc.store(-1);   // native mode in/out: the keys may move to another port
        }
        return;
      }
      if (!(b & 0x80)) return;
      q.sysex = false;                                       // a status byte ends a broken SysEx
    }
    if (b == 0xF7) return;
    if (b & 0x80) { q.run = (b < 0xF0) ? b : 0; q.nd = 0; return; }
    if (!q.run) return;
    q.d[q.nd++] = b;
    const int need = ((q.run & 0xF0) == 0xC0 || (q.run & 0xF0) == 0xD0) ? 1 : 2;
    if (q.nd < need) return;
    q.nd = 0;
    if (Kind() == CTRL_MK && !MkFilter(q.run, q.d[0], si)) return;
    mKnobs.push(((uint32_t)q.run << 16) | ((uint32_t)q.d[0] << 8) | (need > 1 ? q.d[1] : 0));
  }
  // the microKONTROL: only the keys, pitch bend and the modulation wheel play the engine (its other CCs would
  // land on MS-20ic knobs); they are taken from the one port the keys arrive on
  bool MkFilter(uint8_t st, uint8_t d1, int si)
  {
    const int t = st & 0xF0;
    if (t == 0xB0 && d1 != 1) { mMkCC.fetch_add(1); return false; }   // a plain CC: the controller is not in native mode
    if (t != 0x80 && t != 0x90 && t != 0xE0 && t != 0xB0) return false;
    int ns = mNoteSrc.load();
    if (ns < 0 && t == 0x90) { mNoteSrc.store(si); ns = si; }
    if (ns >= 0 && ns != si) return false;
    if (t == 0xE0) mMkBend.store(true);
    if (t == 0xB0) mMkMod.store(true);
    return true;
  }
};

// PanelPort: the plugin's own MIDI port, "Twenty Engine Panel". What the plugin sends here reaches the DAW like
// any MIDI input, so a recording track (input "All") records it into the region. One port per process, shared
// by every instance. Main thread only.
class PanelPort
{
public:
  static PanelPort& Get() { static PanelPort p; return p; }
  bool Ok() const { return mSrc != 0; }
  void Rescan() {}   // the port is made by the plugin itself
  void Send(const uint8_t* bytes, int len)
  {
    if (!mSrc || len <= 0 || len > 200) return;
    Byte buf[512];
    MIDIPacketList* pl = (MIDIPacketList*)buf;
    MIDIPacket* pk = MIDIPacketListInit(pl);
    pk = MIDIPacketListAdd(pl, sizeof(buf), pk, 0, (ByteCount)len, bytes);
    if (pk) MIDIReceived(mSrc, pl);
  }
private:
  PanelPort()
  {
    if (MIDIClientCreate(CFSTR("TwentyEngine Panel"), nullptr, nullptr, &mClient) != noErr) return;
    if (MIDISourceCreate(mClient, CFSTR("Twenty Engine Panel"), &mSrc) != noErr) { mSrc = 0; return; }
    MIDIObjectSetIntegerProperty(mSrc, kMIDIPropertyUniqueID, 0x54455031);   // stable: the DAW remembers the port
  }
  MIDIClientRef mClient = 0;
  MIDIEndpointRef mSrc = 0;
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
#pragma comment(lib, "advapi32.lib")

class LedPort
{
public:
  ~LedPort() { MkNativeOff(); Close(); CloseIn(); CloseMk(); }
  // look for the controller's output; keep it open while it works ("MS-20" for the MS-20 family, the
  // microKONTROL's CTRL port for it).
  // The controller's input is opened for the plugin itself only where Windows lets several programs share a
  // MIDI port (Windows MIDI Services, its service "midisrv" running). Everywhere else (Windows 10, Windows 11
  // without it) a port belongs to one program: if the DAW let go of the iC for a moment, the plugin took it and
  // the DAW could not get it back. There the plugin plays the knobs through the track (as the DAW sends them).
  // The microKONTROL: the plugin opens its ports for the native messages only (encoders, sliders, pads,
  // displays); the keys reach the plugin through the DAW's track as with any keyboard.
  void Rescan()
  {
    RescanInput();
    const int kind = Kind();
    if (mOut && kind == mOutKind) return;
    Close();
    const UINT n = midiOutGetNumDevs();
    int best = -1, bestScore = 0;
    for (UINT i = 0; i < n; i++)
    {
      MIDIOUTCAPSA caps;
      if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
      int score = 0;
      if (kind != CTRL_MK) score = Has(caps.szPname, "ms-20") ? 1 : 0;
      else if (Has(caps.szPname, "microkontrol")) score = Has(caps.szPname, "ctrl") ? 3 : Has(caps.szPname, "midi out") ? 1 : 2;
      if (score > bestScore) { bestScore = score; best = (int)i; }
    }
    if (best < 0) return;
    if (midiOutOpen(&mOut, (UINT)best, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { mOut = nullptr; return; }
    mOutKind = kind;
  }
  void Send(const uint8_t* bytes, int len)
  {
    if (!mOut || len <= 0 || len > 64) return;
    MMRESULT r;
    if (bytes[0] == 0xF0)
    {
      // SysEx: Windows sends it in the background and keeps using the header and the bytes until it marks
      // them done. Each message gets its own slot, reused only once Windows is finished with it; nothing waits.
      // All slots still busy (a burst, e.g. the start-up light show): this LED message is skipped.
      Reclaim();
      int k = 0; while (k < kSlots && mBusy[k]) k++;
      if (k == kSlots) return;
      memcpy(mBufs[k], bytes, len);
      MIDIHDR &h = mHdr[k]; h = MIDIHDR{};
      h.lpData = (LPSTR)mBufs[k]; h.dwBufferLength = (DWORD)len; h.dwBytesRecorded = (DWORD)len;
      r = midiOutPrepareHeader(mOut, &h, sizeof(h));
      if (r == MMSYSERR_NOERROR)
      {
        mBusy[k] = true;
        r = midiOutLongMsg(mOut, &h, sizeof(h));
        if (r != MMSYSERR_NOERROR) { midiOutUnprepareHeader(mOut, &h, sizeof(h)); mBusy[k] = false; }
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
  bool PopNative(uint8_t &g, uint8_t &cmd, uint8_t &a, uint8_t &b) { uint32_t v; if (!mNative.pop(v)) return false; g = (uint8_t)(v >> 24); cmd = (uint8_t)(v >> 16); a = (uint8_t)(v >> 8); b = (uint8_t)v; return true; }
  bool KnobsDirect() const { return mIn != nullptr && mInOk.load(std::memory_order_relaxed); }
  int Kind() const { return mKind.load(std::memory_order_relaxed); }
  bool MkActive() const { return Kind() == CTRL_MK && mOut != nullptr; }
  int MkPlainCC() const { return 0; }          // the keys and CCs go through the DAW here
  bool MkBend() const { return true; }         // the DAW's track carries the standard pitch bend / CC 1
  bool MkMod() const { return true; }

  static int KindOf(const char* name)
  {
    if (Has(name, "twenty engine panel")) return CTRL_NONE;   // the loopMIDI port for PanelPort, never the controller
    if (Has(name, "ksoloti")) return CTRL_KSOLOTI;
    if (Has(name, "teensy") || Has(name, "twenty engine")) return CTRL_TEENSY;
    if (Has(name, "ms-20")) return CTRL_IC;
    if (Has(name, "microkontrol")) return CTRL_MK;
    return CTRL_NONE;
  }
  // Which controller is plugged in (same rules as on the Mac), from the MIDI inputs Windows lists.
  static int FindController()
  {
    int found = 0;
    const UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; i++)
    {
      MIDIINCAPSA caps;
      if (midiInGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
      const int k = KindOf(caps.szPname);
      if (CtrlRank(k) > CtrlRank(found)) found = k;
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
  void Close()
  {
    if (!mOut) return;
    midiOutReset(mOut);                                   // marks every queued SysEx done
    for (int k = 0; k < kSlots; k++)
      if (mBusy[k]) { for (int t = 0; t < 200 && !(mHdr[k].dwFlags & MHDR_DONE); t++) Sleep(1); midiOutUnprepareHeader(mOut, &mHdr[k], sizeof(MIDIHDR)); mBusy[k] = false; }
    midiOutClose(mOut); mOut = nullptr; mOutKind = -1;
  }
  void Reclaim() { for (int k = 0; k < kSlots; k++) if (mBusy[k] && (mHdr[k].dwFlags & MHDR_DONE)) { midiOutUnprepareHeader(mOut, &mHdr[k], sizeof(MIDIHDR)); mBusy[k] = false; } }
  static const int kSlots = 16;
  MIDIHDR mHdr[kSlots] = {}; char mBufs[kSlots][64] = {}; bool mBusy[kSlots] = {};
  HMIDIOUT mOut = nullptr; int mOutKind = -1;
  std::atomic<int> mKind{0}, mMkG{-1};
  KnobQueue mNative;
  void MkNativeOff()
  {
    const int g = mMkG.load();
    if (Kind() != CTRL_MK || !mOut || g < 0) return;
    const uint8_t m[9] = {0xF0, 0x42, (uint8_t)(0x40 | g), 0x6E, 0x00, 0x00, 0x00, 0x00, 0xF7};
    Send(m, 9);
    for (int t = 0; t < 50 && mBusy[0]; t++) { Sleep(1); Reclaim(); }
  }
  // Windows MIDI Services running? (checked once: the service does not come and go while the DAW runs)
  static bool SharedPorts()
  {
    static int known = -1;
    if (known >= 0) return known == 1;
    known = 0;
    if (SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT))
    {
      if (SC_HANDLE sv = OpenServiceA(scm, "midisrv", SERVICE_QUERY_STATUS))
      {
        SERVICE_STATUS st;
        if (QueryServiceStatus(sv, &st) && st.dwCurrentState == SERVICE_RUNNING) known = 1;
        CloseServiceHandle(sv);
      }
      CloseServiceHandle(scm);
    }
    return known == 1;
  }
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
      const int k = KindOf(caps.szPname);
      if (CtrlRank(k) > CtrlRank(bestK)) { bestK = k; best = (int)i; }
    }
    if (bestK != Kind()) mMkG.store(-1);
    mKind.store(bestK);
    if (bestK == CTRL_MK) { CloseIn(); RescanMk(); return; }
    CloseMk();
    if (!SharedPorts()) { CloseIn(); return; }   // Windows 10: the DAW's port, never taken (see Rescan)
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
  // ---- the microKONTROL's ports: SysEx in (native messages). Every one of its inputs the DAW does not hold
  //      is opened (the native messages come on PORT A; which ones Windows lets us open depends on the DAW).
  static const int kMkIns = 4, kMkBufs = 4;
  struct MkIn { LedPort *self = nullptr; HMIDIIN h = nullptr; int dev = -1; std::atomic<bool> closing{false};
                MIDIHDR hdr[kMkBufs] = {}; char buf[kMkBufs][256] = {}; uint8_t sx[16] = {}; int nsx = 0; };
  MkIn mMk[kMkIns];
  void CloseMk()
  {
    for (int i = 0; i < kMkIns; i++)
    {
      MkIn &m = mMk[i];
      if (!m.h) continue;
      m.closing.store(true);
      midiInStop(m.h); midiInReset(m.h);                     // gives the buffers back (MIM_LONGDATA, 0 bytes)
      for (int b = 0; b < kMkBufs; b++) midiInUnprepareHeader(m.h, &m.hdr[b], sizeof(MIDIHDR));
      midiInClose(m.h); m.h = nullptr; m.dev = -1;
    }
  }
  void RescanMk()
  {
    const UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; i++)
    {
      MIDIINCAPSA caps;
      if (midiInGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR || KindOf(caps.szPname) != CTRL_MK) continue;
      bool open = false; int freeSlot = -1;
      for (int k = 0; k < kMkIns; k++) { if (mMk[k].h && mMk[k].dev == (int)i) open = true; if (!mMk[k].h && freeSlot < 0) freeSlot = k; }
      if (open || freeSlot < 0) continue;
      MkIn &m = mMk[freeSlot];
      m.self = this; m.closing.store(false); m.nsx = 0;
      if (midiInOpen(&m.h, i, (DWORD_PTR)&LedPort::MkProc, (DWORD_PTR)&m, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) { m.h = nullptr; continue; }
      bool ok = true;
      for (int b = 0; b < kMkBufs && ok; b++)
      {
        m.hdr[b] = MIDIHDR{}; m.hdr[b].lpData = m.buf[b]; m.hdr[b].dwBufferLength = sizeof(m.buf[b]);
        ok = midiInPrepareHeader(m.h, &m.hdr[b], sizeof(MIDIHDR)) == MMSYSERR_NOERROR && midiInAddBuffer(m.h, &m.hdr[b], sizeof(MIDIHDR)) == MMSYSERR_NOERROR;
      }
      if (!ok || midiInStart(m.h) != MMSYSERR_NOERROR) { m.dev = (int)i; CloseMkOne(m); continue; }
      m.dev = (int)i;
    }
  }
  void CloseMkOne(MkIn &m)
  {
    m.closing.store(true); midiInReset(m.h);
    for (int b = 0; b < kMkBufs; b++) midiInUnprepareHeader(m.h, &m.hdr[b], sizeof(MIDIHDR));
    midiInClose(m.h); m.h = nullptr; m.dev = -1;
  }
  static void CALLBACK MkProc(HMIDIIN h, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR)
  {
    if (msg != MIM_LONGDATA) return;
    MkIn &m = *(MkIn *)inst;
    MIDIHDR *hd = (MIDIHDR *)p1;
    for (DWORD i = 0; i < hd->dwBytesRecorded; i++)
    {
      const uint8_t b = (uint8_t)hd->lpData[i];
      if (b == 0xF0) m.nsx = 0;
      if (m.nsx < (int)sizeof(m.sx)) m.sx[m.nsx] = b;
      m.nsx++;
      if (b == 0xF7)
      {
        uint32_t v;
        if (MkNativeMsg(m.sx, m.nsx, v)) { m.self->mMkG.store((int)(v >> 24)); m.self->mNative.push(v); }
        m.nsx = 0;
      }
    }
    if (!m.closing.load() && hd->dwBytesRecorded > 0) midiInAddBuffer(h, hd, sizeof(MIDIHDR));   // back in the queue
  }
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
  bool PopNative(uint8_t &, uint8_t &, uint8_t &, uint8_t &) { return false; }
  int Kind() const { return 0; }
  bool MkActive() const { return false; }
  int MkPlainCC() const { return 0; }
  bool MkBend() const { return false; }
  bool MkMod() const { return false; }
  static int FindController() { return 0; }
};
#endif

#if defined(_WIN32)
// PanelPort on Windows: a plugin cannot make a MIDI port of its own on Windows 10, so the port is made with the
// free loopMIDI (Tobias Erichsen): a port named "Twenty Engine Panel". The plugin sends into it; the DAW sees the
// same port as a MIDI input and records it with the track (input: all MIDI inputs), exactly as on the Mac.
// Not there: Ok() is false and the plugin plays the panel's sound as before. Main thread only.
class PanelPort
{
public:
  static PanelPort& Get() { static PanelPort p; return p; }
  bool Ok() const { return mOut != nullptr; }
  // look for the port (again): the user may create it while the DAW runs
  void Rescan()
  {
    if (mOut) return;
    const UINT n = midiOutGetNumDevs();
    for (UINT i = 0; i < n; i++)
    {
      MIDIOUTCAPSA caps;
      if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
      char low[64]; size_t k = 0; for (; caps.szPname[k] && k < sizeof(low) - 1; k++) low[k] = (char)tolower((unsigned char)caps.szPname[k]); low[k] = 0;
      if (!strstr(low, "twenty engine panel")) continue;
      if (midiOutOpen(&mOut, i, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) mOut = nullptr;
      return;
    }
  }
  void Send(const uint8_t* b, int len)
  {
    if (!mOut || len < 2 || len > 3) return;   // the stream is short messages only
    const DWORD m = (DWORD)b[0] | ((DWORD)b[1] << 8) | (len > 2 ? ((DWORD)b[2] << 16) : 0);
    if (midiOutShortMsg(mOut, m) != MMSYSERR_NOERROR) { midiOutClose(mOut); mOut = nullptr; }   // the port went away
  }
private:
  PanelPort() { Rescan(); }
  ~PanelPort() { if (mOut) { midiOutReset(mOut); midiOutClose(mOut); } }
  HMIDIOUT mOut = nullptr;
};
#elif !defined(__APPLE__)
class PanelPort { public: static PanelPort& Get() { static PanelPort p; return p; } bool Ok() const { return false; } void Send(const uint8_t*, int) {} void Rescan() {} };
#endif
