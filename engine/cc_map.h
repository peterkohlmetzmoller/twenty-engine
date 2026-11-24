// cc_map.h - CC learn: which MIDI controller moves which parameter.
// One map for the whole setup (every copy of the plugin, every project), kept in ccmap.txt next to the sound
// memories: a learned controller belongs to the user's studio, not to a sound, so it is never inside a preset.
// The parameter is stored by its patch name (object + parameter), so the map survives new parameters.
// In MS-20ic mode the iC's own controllers stay as they are: their CC numbers cannot be learned, and a learned
// map never acts on them (the iC's knobs would otherwise move two things).
#pragma once
#include <atomic>
#include <cstdio>
#include <cstring>
#include "engine_gen.h"

namespace ms20 {

// the controllers the MS-20ic (and the Ksoloti / Teensy inside it) sends: knobs, ESP knobs, cables (NRPN), volume,
// wheel, the Twenty Engine buttons, the memory keys sent to the computer
inline bool IcOwnCc(int cc)
{
  static const unsigned char kIc[] = { 1, 5, 6, 7, 11, 12, 14, 15, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    70, 71, 72, 73, 74, 75, 76, 77, 79, 82, 85, 88, 89, 90, 91, 93, 98, 99, 102, 103, 104, 105, 106 };
  for (unsigned char c : kIc) if (c == cc) return true;
  return false;
}

struct CcMap
{
  std::atomic<int> param[128];     // CC -> parameter index, -1 none
  std::atomic<int> learning{-1};   // a parameter waiting for a controller (-1 none)
  std::atomic<int> learnedCc{-1};  // the last CC learned (for the window's message), -1 none
  std::atomic<int> changed{0};     // bumped on every change: the map file is written on the main thread
  CcMap() { for (auto& p : param) p.store(-1); }

  int ccOf(int p) const { for (int c = 0; c < 128; c++) if (param[c].load() == p) return c; return -1; }
  void forget(int p) { for (int c = 0; c < 128; c++) if (param[c].load() == p) param[c].store(-1); changed++; }
  // a CC can be learned unless it is a mode change / all notes off (120..127), or the iC's own in MS-20ic mode
  static bool learnable(int cc, bool icMode) { return cc >= 0 && cc < 120 && !(icMode && IcOwnCc(cc)); }

  // audio thread: a controller from a keyboard or the DAW. Learns it if a parameter waits for one. Returns the
  // parameter it moves (-1: not learned - the engine gets it as before).
  int onCc(int cc, bool icMode)
  {
    if (cc < 0 || cc > 127) return -1;
    const int lp = learning.load();
    if (lp >= 0 && learnable(cc, icMode))
    {
      for (int c = 0; c < 128; c++) if (param[c].load() == lp) param[c].store(-1);   // one controller per parameter
      param[cc].store(lp); learning.store(-1); learnedCc.store(cc); changed++;
    }
    const int p = param[cc].load();
    if (p < 0 || (icMode && IcOwnCc(cc))) return -1;
    return p;
  }

  // ccmap.txt: "CC object parameter" per line (e.g. "21 Filter_Macro lpfcut")
  bool load(const char* path)
  {
    FILE* f = fopen(path, "r"); if (!f) return false;
    for (auto& p : param) p.store(-1);
    char line[200];
    while (fgets(line, sizeof(line), f))
    {
      int cc = -1; char obj[80] = "", nm[80] = "";
      if (sscanf(line, "%d %79s %79s", &cc, obj, nm) != 3 || cc < 0 || cc > 127) continue;
      for (int i = 0; i < NPARAMS; i++) if (!strcmp(kParamInfo[i].object, obj) && !strcmp(kParamInfo[i].name, nm)) { param[cc].store(i); break; }
    }
    fclose(f); return true;
  }
  bool save(const char* path) const
  {
    FILE* f = fopen(path, "w"); if (!f) return false;
    fputs("# Twenty Engine: learned MIDI controllers (CC, then the parameter). Right-click a knob to change them.\n", f);
    for (int c = 0; c < 128; c++) { const int p = param[c].load(); if (p >= 0 && p < NPARAMS) fprintf(f, "%d %s %s\n", c, kParamInfo[p].object, kParamInfo[p].name); }
    fclose(f); return true;
  }
};

// the one map of this process (every copy of the plugin uses it)
inline CcMap& GlobalCcMap() { static CcMap m; return m; }

} // namespace ms20
