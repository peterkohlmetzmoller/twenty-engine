// StudioHost.h - what the panel window (Twenty Engine UI) needs from the plugin (no graphics code, so the plugin can
// implement it in every build).
#pragma once
namespace te_ui {
struct PatchHost {
  virtual ~PatchHost() {}
  virtual bool CableFlag(int which) const = 0;           // 0 mono (KBD CV -> VCO 1+2 CV IN), 1 tap (SWITCH -> ESP SIGNAL IN)
  virtual void SetCableFlag(int which, bool on) = 0;     // sends the MS-20ic's own cable messages to the engine
  virtual int NoisePick(int jackIndex) const = 0;        // which noise jack a noise cable comes from (0 pink, 1 white)
  virtual void SetNoisePick(int jackIndex, int pick) = 0;
  virtual void SwitchDown(bool down) = 0;               // the MS-20's momentary switch (tap tempo)
  virtual int Leds() const { return 0x10; }             // the LEDs as the engine drives them (bit 0 MG rate, 1 EG 1, 2 PEAK, 3 TRIG, 4 power)
  // arp SYNC to the host's tempo: 0 free, 1..6 = 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32; -1 = not available (no host clock)
  virtual int ArpSync() const { return -1; }
  virtual void SetArpSync(int) {}
};
}
