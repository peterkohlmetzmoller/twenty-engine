// StudioHost.h - what the panel window (Twenty Engine UI) needs from the plugin (no graphics code, so the plugin can
// implement it in every build).
#pragma once
#ifndef TWENTY_SUPPORT_MAIL
#define TWENTY_SUPPORT_MAIL "twentyenginesupport@gmail.com"   // where bug logs go
#endif
namespace te_ui {

// while SYNC is on, the RATE knob (0..1) picks the division in six equal zones, slow to fast
static const char* const kSyncName[6] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
static const double kSyncBeats[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
inline int SyncZone(double rate01) { int z = (int)(rate01 * 6.0); return z < 0 ? 0 : (z > 5 ? 5 : z); }
struct PatchHost {
  virtual ~PatchHost() {}
  virtual bool CableFlag(int which) const = 0;           // 0 mono (KBD CV -> VCO 1+2 CV IN), 1 tap (SWITCH -> ESP SIGNAL IN)
  virtual void SetCableFlag(int which, bool on) = 0;     // sends the MS-20ic's own cable messages to the engine
  virtual int NoisePick(int jackIndex) const = 0;        // which noise jack a noise cable comes from (0 pink, 1 white)
  virtual void SetNoisePick(int jackIndex, int pick) = 0;
  virtual void SwitchDown(bool down) = 0;               // the MS-20's momentary switch (tap tempo)
  virtual int Leds() const { return 0x10; }             // the LEDs as the engine drives them (bit 0 MG rate, 1 EG 1, 2 PEAK, 3 TRIG, 4 power)
  // arp SYNC switch: 1 = the arp steps on the host's beat, the RATE knob picks the division; 0 = free (RATE = speed);
  // -1 = not available (no host clock)
  virtual int ArpSync() const { return -1; }
  virtual void SetArpSync(int) {}
  // the plugin's mode (Twenty Engine UI in a DAW): 0 automatic (MS-20ic mode while an iC is connected), 1 MS-20ic,
  // 2 standard synth plugin. -1: no modes (the standalone apps). IcMode: the mode in effect.
  virtual int ModeSetting() const { return -1; }
  virtual void SetModeSetting(int) {}
  virtual bool IcMode() const { return true; }
  virtual void CopyLog() {}                                      // the bug log: to the clipboard (and a file)
  virtual bool LogSavedToFile() const { return true; }           // the web page has no file
  virtual void MailLog() {}                                      // a new email to the support address, the setup filled in
  virtual void PatchCable(int outJack, int inJack, bool on) {}   // the MS-20ic's own cable message (jack numbers as it sends them)
  virtual void SetEspLatch(bool on) {}                           // the DELAY latch: the iC's ESP knobs follow it (engine CC 105)
  // CC learn (engine/cc_map.h: one map for the setup). CcOf: the controller learned for a parameter, -1 none.
  // CcLearn(p): the next controller moved is learned for p (-1: stop). CcTakeLearned: a controller just learned
  // (once, -1 none), for the window's message.
  virtual bool CcAvailable() const { return false; }
  virtual int CcOf(int param) const { return -1; }
  virtual int CcLearning() const { return -1; }
  virtual void CcLearn(int param) {}
  virtual void CcForget(int param) {}
  virtual int CcTakeLearned() { return -1; }
  // the standalone apps: every take is saved as a WAV (engine/capture.h); the status line shows it, a click opens
  // the folder
  virtual bool HasCapture() const { return false; }
  virtual void ShowCaptures() {}
};
}
