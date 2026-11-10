// StudioHost.h - what the Studio editor window needs from the plugin (no graphics code, so the plugin can
// implement it in every build).
#pragma once
namespace te_ui {
struct PatchHost {
  virtual ~PatchHost() {}
  virtual bool CableFlag(int which) const = 0;           // 0 mono (KBD CV -> VCO 1+2 CV IN), 1 tap (SWITCH -> ESP SIGNAL IN)
  virtual void SetCableFlag(int which, bool on) = 0;     // sends the MS-20ic's own cable messages to the engine
  virtual int NoisePick(int jackIndex) const = 0;        // which noise jack a noise cable comes from (0 pink, 1 white)
  virtual void SetNoisePick(int jackIndex, int pick) = 0;
};
struct MemoryHost {
  virtual ~MemoryHost() {}
  virtual void MemRecall(int n) = 0;            // 0..35, 36 = the panel
  virtual void MemSave(int n) = 0;
  virtual bool MemValid(int n) const = 0;
  virtual int MemSlot() const = 0;              // -1 = the panel
  virtual const char* MemName(int n) const = 0;
  virtual void SetMemName(int n, const char* name) = 0;
  virtual bool MemImport(const char* path) = 0;
  virtual bool MemExport(const char* path) = 0;
  virtual void SwitchDown(bool down) = 0;       // the MS-20's momentary switch
};
}
