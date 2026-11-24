// alsa_test.cpp - the Linux controller port (led_port.h, ALSA sequencer) against a pretend MS-20ic and a pretend
// keyboard, both ALSA clients made here. Needs the ALSA sequencer (snd-seq kernel module).
// Build: g++ -std=c++17 -O2 -DTWENTY_ALSA -I.. -o alsa_test alsa_test.cpp -lasound -lpthread && ./alsa_test
#include "../led_port.h"
#include <chrono>
#include <cstdio>
#include <thread>

static int fails = 0;
#define CHECK(c, what) do { if (c) printf("ok    %s\n", what); else { printf("FAIL  %s\n", what); fails++; } } while (0)
static void Wait(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

struct Fake
{
  snd_seq_t* h = nullptr; int port = -1, client = -1;
  Fake(const char* name, unsigned type)
  {
    if (snd_seq_open(&h, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) { h = nullptr; return; }
    snd_seq_set_client_name(h, name);
    port = snd_seq_create_simple_port(h, name, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, type);
    client = snd_seq_client_id(h);
  }
  ~Fake() { if (h) snd_seq_close(h); }
  void Send(snd_seq_event_t& ev) { snd_seq_ev_set_source(&ev, port); snd_seq_ev_set_subs(&ev); snd_seq_ev_set_direct(&ev); snd_seq_event_output_direct(h, &ev); }
  void Note(int ch, int key, int vel) { snd_seq_event_t ev; snd_seq_ev_clear(&ev); snd_seq_ev_set_noteon(&ev, ch, key, vel); Send(ev); }
  void CC(int ch, int cc, int v) { snd_seq_event_t ev; snd_seq_ev_clear(&ev); snd_seq_ev_set_controller(&ev, ch, cc, v); Send(ev); }
  void Pressure(int ch, int v) { snd_seq_event_t ev; snd_seq_ev_clear(&ev); snd_seq_ev_set_chanpress(&ev, ch, v); Send(ev); }
  // what reached this client: count of SysEx messages and the last one's bytes
  int sysex = 0; uint8_t last[16] = {}; int lastLen = 0;
  void Read()
  {
    snd_seq_event_t* ev = nullptr;
    while (snd_seq_event_input(h, &ev) >= 0 && ev)
      if (ev->type == SND_SEQ_EVENT_SYSEX) { sysex++; lastLen = (int)std::min<unsigned>(16, ev->data.ext.len); memcpy(last, ev->data.ext.ptr, lastLen); }
  }
};

int main()
{
  snd_seq_t* probe = nullptr;
  if (snd_seq_open(&probe, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) { printf("SKIP  no ALSA sequencer here\n"); return 0; }
  snd_seq_close(probe);

  CHECK(LedPort::FindController() == 0, "no controller before the pretend MS-20ic exists");
  Fake ic("MS-20 Controller", SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_HARDWARE);
  Fake kb("Test Keyboard", SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_HARDWARE);
  CHECK(ic.h && kb.h, "pretend MS-20ic and keyboard made");
  CHECK(LedPort::FindController() == 1, "FindController sees an MS-20ic");

  LedPort port;
  port.Rescan(); Wait(100);
  CHECK(port.KnobsDirect(), "connected to the controller (knobs direct)");
  CHECK(port.Connected(), "LED output connected");

  // controller -> knob queue
  ic.CC(0, 74, 100); ic.Note(0, 60, 90); ic.Pressure(0, 127);
  Wait(150);
  uint8_t s, d1, d2; int n = 0; bool cc = false, note = false, pr = false;
  while (port.PopKnob(s, d1, d2)) { n++; if (s == 0xB0 && d1 == 74 && d2 == 100) cc = true; if (s == 0x90 && d1 == 60 && d2 == 90) note = true; if (s == 0xD0 && d1 == 127) pr = true; }
  CHECK(cc && note && pr && n == 3, "controller: CC, note, pressure arrive as the controller's (knob queue)");
  CHECK(!port.PopKey(s, d1, d2), "controller messages are not taken as a keyboard's");

  // cable message (CC 99/98/6) arrives as three CCs, in order
  ic.CC(0, 99, 4); ic.CC(0, 98, 31); ic.CC(0, 6, 25); Wait(150);
  int seq[3] = {0, 0, 0}, k = 0; while (port.PopKnob(s, d1, d2) && k < 3) seq[k++] = d1;
  CHECK(k == 3 && seq[0] == 99 && seq[1] == 98 && seq[2] == 6, "cable message: CC 99, 98, 6 in order");

  // keyboard -> key queue
  kb.Note(2, 64, 80); Wait(150);
  bool key = false; while (port.PopKey(s, d1, d2)) if (s == 0x92 && d1 == 64 && d2 == 80) key = true;
  CHECK(key, "other keyboard: note arrives as a key");
  CHECK(!port.PopKnob(s, d1, d2), "keyboard notes are not taken as the controller's");

  // LEDs: SysEx reaches the controller only
  const uint8_t led[9] = {0xF0, 0x42, 0x40, 0x6E, 0x01, 0x01, 0x00, 0x3F, 0xF7};
  for (int i = 0; i < 5; i++) port.Send(led, 9);
  Wait(150); ic.Read(); kb.Read();
  CHECK(ic.sysex == 5 && ic.lastLen == 9 && !memcmp(ic.last, led, 9), "LED SysEx reaches the MS-20ic, unchanged");
  CHECK(kb.sysex == 0, "LED SysEx does not go to the keyboard");

  // unplug: the controller goes away
  { snd_seq_close(ic.h); ic.h = nullptr; }
  Wait(100); port.Rescan(); Wait(50);
  CHECK(!port.KnobsDirect(), "controller unplugged: not direct any more");
  CHECK(LedPort::FindController() == 0, "FindController: none");

  printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
  return fails ? 1 : 0;
}
