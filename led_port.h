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
      if (has(CFSTR("Twenty Engine Panel"))) k = 0;   // our own port (PanelPort), never the controller
      else if (has(CFSTR("Ksoloti"))) k = 2;
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
      if (has(CFSTR("Twenty Engine Panel"))) k = 0;   // our own port (PanelPort), never the controller
      else if (has(CFSTR("Ksoloti"))) k = 2;
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

// PanelPort: the plugin's own MIDI port, "Twenty Engine Panel". What the plugin sends here reaches the DAW like
// any MIDI input, so a recording track (input "All") records it into the region. One port per process, shared
// by every instance. Main thread only.
class PanelPort
{
public:
  static PanelPort& Get() { static PanelPort p; return p; }
  bool Ok() const { return mSrc != 0; }
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
  void Close()
  {
    if (!mOut) return;
    midiOutReset(mOut);                                   // marks every queued SysEx done
    for (int k = 0; k < kSlots; k++)
      if (mBusy[k]) { for (int t = 0; t < 200 && !(mHdr[k].dwFlags & MHDR_DONE); t++) Sleep(1); midiOutUnprepareHeader(mOut, &mHdr[k], sizeof(MIDIHDR)); mBusy[k] = false; }
    midiOutClose(mOut); mOut = nullptr;
  }
  void Reclaim() { for (int k = 0; k < kSlots; k++) if (mBusy[k] && (mHdr[k].dwFlags & MHDR_DONE)) { midiOutUnprepareHeader(mOut, &mHdr[k], sizeof(MIDIHDR)); mBusy[k] = false; } }
  static const int kSlots = 16;
  MIDIHDR mHdr[kSlots] = {}; char mBufs[kSlots][64] = {}; bool mBusy[kSlots] = {};
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
};

#elif defined(__linux__) && defined(TWENTY_ALSA)
// Linux (the standalone app): the ALSA sequencer, which every Linux audio setup has (also under JACK or PipeWire).
// One client "Twenty Engine" with an input port that is connected to the controller (knobs, cables, keys: the
// live engine, like the hardware) and to every other MIDI keyboard (keys only), and an output port for the LEDs.
// Two handles: the reader thread owns the input one; the output one (sending, connecting) is used by one other
// thread only (the app's idle thread).
#include <alsa/asoundlib.h>
#include <poll.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <thread>
#include <vector>

class LedPort
{
public:
  LedPort()
  {
    if (snd_seq_open(&mOut, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) { mOut = nullptr; return; }
    if (snd_seq_open(&mIn, "default", SND_SEQ_OPEN_INPUT, 0) < 0) { mIn = nullptr; return; }
    // named like the Mac's own port: never taken for a Twenty Engine controller (Teensy, Ksoloti)
    snd_seq_set_client_name(mOut, "Twenty Engine Panel");
    snd_seq_set_client_name(mIn, "Twenty Engine Panel (in)");
    mOutPort = snd_seq_create_simple_port(mOut, "LEDs out", SND_SEQ_PORT_CAP_READ, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    mInPort = snd_seq_create_simple_port(mIn, "Keys and knobs in", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    mOutClient = snd_seq_client_id(mOut); mInClient = snd_seq_client_id(mIn);
    snd_midi_event_new(256, &mEnc);
    mRun.store(true);
    mThread = std::thread([this] { Reader(); });
  }
  ~LedPort()
  {
    mRun.store(false);
    if (mThread.joinable()) mThread.join();
    if (mEnc) snd_midi_event_free(mEnc);
    if (mIn) snd_seq_close(mIn);
    if (mOut) snd_seq_close(mOut);
  }
  bool PopKnob(uint8_t &s, uint8_t &d1, uint8_t &d2) { uint32_t v; if (!mKnobs.pop(v)) return false; s = v >> 16; d1 = v >> 8; d2 = v; return true; }
  bool PopKey(uint8_t &s, uint8_t &d1, uint8_t &d2) { uint32_t v; if (!mKeys.pop(v)) return false; s = v >> 16; d1 = v >> 8; d2 = v; return true; }
  bool KnobsDirect() const { return mDirect.load(std::memory_order_relaxed); }
  bool Connected() const { return mDest.client >= 0; }

  // the controller (best match by name), connected both ways; every other keyboard into the input
  void Rescan()
  {
    if (!mOut || !mIn) return;
    std::vector<Port> ports; List(mOut, ports, mOutClient, mInClient);
    Port best; int bestK = 0;
    for (const Port &p : ports) if (p.kind > bestK && p.readable) { bestK = p.kind; best = p; }
    const int src = bestK ? best.client : -1, sport = bestK ? best.port : -1;
    if (src != mSrc.client || sport != mSrc.port)
    {
      if (mSrc.client >= 0) Unsubscribe(mSrc.client, mSrc.port, mInClient, mInPort);
      if (mDest.client >= 0) Unsubscribe(mOutClient, mOutPort, mDest.client, mDest.port);
      mSrc = {-1, -1}; mDest = {-1, -1};
      if (src >= 0 && Subscribe(src, sport, mInClient, mInPort)) mSrc = {src, sport};
      // the LEDs go to the controller's writable port (the same port on USB MIDI devices)
      for (const Port &p : ports)
        if (p.client == src && p.writable && Subscribe(mOutClient, mOutPort, p.client, p.port)) { mDest = {p.client, p.port}; break; }
      mCtlClient.store(mSrc.client);
    }
    mDirect.store(mSrc.client >= 0);
    // keyboards: every other hardware port that sends MIDI (connecting twice is refused by ALSA, harmless)
    for (const Port &p : ports)
      if (p.kind == 0 && p.readable && p.hardware && p.client != mSrc.client) Subscribe(p.client, p.port, mInClient, mInPort);
  }
  void Send(const uint8_t* bytes, int len)
  {
    if (!mOut || mDest.client < 0 || len <= 0 || len > 64) return;
    snd_seq_event_t ev; snd_seq_ev_clear(&ev);
    if (bytes[0] == 0xF0) snd_seq_ev_set_sysex(&ev, (unsigned)len, (void*)bytes);
    else { snd_midi_event_reset_encode(mEnc); if (snd_midi_event_encode(mEnc, bytes, len, &ev) <= 0 || ev.type == SND_SEQ_EVENT_NONE) return; }
    snd_seq_ev_set_source(&ev, mOutPort);
    snd_seq_ev_set_dest(&ev, mDest.client, mDest.port);
    snd_seq_ev_set_direct(&ev);
    snd_seq_event_output_direct(mOut, &ev);
  }

  // Which controller is plugged in: 0 none, 1 stock MS-20 iC, 2 Ksoloti, 3 Teensy (same rules as Mac and Windows)
  static int FindController()
  {
    snd_seq_t* h = nullptr;
    if (snd_seq_open(&h, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) return 0;
    std::vector<Port> ports; List(h, ports, snd_seq_client_id(h), -1);
    snd_seq_close(h);
    int found = 0; for (const Port &p : ports) if (p.readable && p.kind > found) found = p.kind;
    return found;
  }

private:
  struct Addr { int client, port; };
  struct Port { int client = -1, port = -1, kind = 0; bool readable = false, writable = false, hardware = false; };
  static bool Has(const std::string& s, const char* lowerNeedle)
  {
    std::string low(s); for (char& c : low) c = (char)tolower((unsigned char)c);
    return low.find(lowerNeedle) != std::string::npos;
  }
  static void List(snd_seq_t* h, std::vector<Port>& out, int self1, int self2)
  {
    snd_seq_client_info_t* ci; snd_seq_port_info_t* pi;
    snd_seq_client_info_alloca(&ci); snd_seq_port_info_alloca(&pi);
    snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(h, ci) >= 0)
    {
      const int c = snd_seq_client_info_get_client(ci);
      if (c == 0 || c == self1 || c == self2) continue;   // System; this app
      const std::string cname = snd_seq_client_info_get_name(ci);
      if (Has(cname, "midi through")) continue;
      snd_seq_port_info_set_client(pi, c); snd_seq_port_info_set_port(pi, -1);
      while (snd_seq_query_next_port(h, pi) >= 0)
      {
        const unsigned cap = snd_seq_port_info_get_capability(pi), type = snd_seq_port_info_get_type(pi);
        if (cap & SND_SEQ_PORT_CAP_NO_EXPORT) continue;
        const std::string name = cname + " " + snd_seq_port_info_get_name(pi);
        Port p; p.client = c; p.port = snd_seq_port_info_get_port(pi);
        p.readable = (cap & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) == (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ);
        p.writable = (cap & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) == (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE);
        p.hardware = (type & (SND_SEQ_PORT_TYPE_HARDWARE | SND_SEQ_PORT_TYPE_PORT)) != 0;
        if (Has(name, "twenty engine panel")) p.kind = 0;
        else if (Has(name, "ksoloti")) p.kind = 2;
        else if (Has(name, "teensy") || Has(name, "twenty engine")) p.kind = 3;
        else if (Has(name, "ms-20") || Has(name, "ms20")) p.kind = 1;
        out.push_back(p);
      }
    }
  }
  bool Subscribe(int sc, int sp, int dc, int dp)
  {
    snd_seq_port_subscribe_t* sub; snd_seq_port_subscribe_alloca(&sub);
    snd_seq_addr_t s{(unsigned char)sc, (unsigned char)sp}, d{(unsigned char)dc, (unsigned char)dp};
    snd_seq_port_subscribe_set_sender(sub, &s); snd_seq_port_subscribe_set_dest(sub, &d);
    const int r = snd_seq_subscribe_port(mOut, sub);
    return r >= 0 || r == -EBUSY;   // EBUSY: already connected
  }
  void Unsubscribe(int sc, int sp, int dc, int dp)
  {
    snd_seq_port_subscribe_t* sub; snd_seq_port_subscribe_alloca(&sub);
    snd_seq_addr_t s{(unsigned char)sc, (unsigned char)sp}, d{(unsigned char)dc, (unsigned char)dp};
    snd_seq_port_subscribe_set_sender(sub, &s); snd_seq_port_subscribe_set_dest(sub, &d);
    snd_seq_unsubscribe_port(mOut, sub);
  }
  // reader thread: events into raw messages; the controller's go to the knob queue, other keyboards' to the keys
  void Reader()
  {
    const int n = snd_seq_poll_descriptors_count(mIn, POLLIN);
    std::vector<pollfd> fds((size_t)(n > 0 ? n : 1));
    snd_seq_poll_descriptors(mIn, fds.data(), (unsigned)fds.size(), POLLIN);
    while (mRun.load())
    {
      if (poll(fds.data(), fds.size(), 100) <= 0) continue;
      snd_seq_event_t* ev = nullptr;
      while (snd_seq_event_input(mIn, &ev) >= 0 && ev)
      {
        const bool ctl = ev->source.client == mCtlClient.load();
        KnobQueue& q = ctl ? mKnobs : mKeys;
        const uint8_t ch = (uint8_t)(ev->data.control.channel & 15);
        auto put = [&](uint8_t s, uint8_t a, uint8_t b) { q.push(((uint32_t)s << 16) | ((uint32_t)(a & 0x7F) << 8) | (b & 0x7F)); };
        switch (ev->type)
        {
          case SND_SEQ_EVENT_NOTEON:     put(0x90 | ev->data.note.channel, ev->data.note.note, ev->data.note.velocity); break;
          case SND_SEQ_EVENT_NOTEOFF:    put(0x80 | ev->data.note.channel, ev->data.note.note, ev->data.note.velocity); break;
          case SND_SEQ_EVENT_KEYPRESS:   put(0xA0 | ev->data.note.channel, ev->data.note.note, ev->data.note.velocity); break;
          case SND_SEQ_EVENT_CONTROLLER: put(0xB0 | ch, (uint8_t)ev->data.control.param, (uint8_t)ev->data.control.value); break;
          case SND_SEQ_EVENT_PGMCHANGE:  put(0xC0 | ch, (uint8_t)ev->data.control.value, 0); break;
          case SND_SEQ_EVENT_CHANPRESS:  put(0xD0 | ch, (uint8_t)ev->data.control.value, 0); break;
          case SND_SEQ_EVENT_PITCHBEND:  { const int v = ev->data.control.value + 8192; put(0xE0 | ch, (uint8_t)(v & 0x7F), (uint8_t)(v >> 7)); break; }
          case SND_SEQ_EVENT_CONTROL14:  { const int v = ev->data.control.value; put(0xB0 | ch, (uint8_t)ev->data.control.param, (uint8_t)(v >> 7)); if (ev->data.control.param < 32) put(0xB0 | ch, (uint8_t)(ev->data.control.param + 32), (uint8_t)v); break; }
          case SND_SEQ_EVENT_NONREGPARAM: case SND_SEQ_EVENT_REGPARAM:
          {
            // ALSA joined CC 99/98/6 (or 101/100/6): split again, the engine reads the MS-20ic's cable messages as CCs
            const bool nrpn = ev->type == SND_SEQ_EVENT_NONREGPARAM; const int prm = ev->data.control.param, v = ev->data.control.value;
            put(0xB0 | ch, nrpn ? 99 : 101, (uint8_t)(prm >> 7)); put(0xB0 | ch, nrpn ? 98 : 100, (uint8_t)prm);
            put(0xB0 | ch, 6, (uint8_t)(v >> 7)); if (v & 0x7F) put(0xB0 | ch, 38, (uint8_t)v);
            break;
          }
          default: break;
        }
      }
    }
  }
  snd_seq_t *mOut = nullptr, *mIn = nullptr;
  int mOutPort = -1, mInPort = -1, mOutClient = -1, mInClient = -1;
  snd_midi_event_t* mEnc = nullptr;
  Addr mSrc{-1, -1}, mDest{-1, -1};
  std::atomic<int> mCtlClient{-1};
  std::atomic<bool> mDirect{false}, mRun{false};
  KnobQueue mKnobs, mKeys;
  std::thread mThread;
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

#ifndef __APPLE__
// PanelPort: Mac only for now (Windows needs Windows MIDI Services for a port of our own)
class PanelPort { public: static PanelPort& Get() { static PanelPort p; return p; } bool Ok() const { return false; } void Send(const uint8_t*, int) {} };
#endif
