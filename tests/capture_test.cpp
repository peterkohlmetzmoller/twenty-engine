// capture_test.cpp - the apps' audio capture (engine/capture.h, keep-all): a take starts at the first note, ends
// after two seconds of silence, is a valid 24-bit WAV of what was played, and every take is kept.
#include "../engine/capture.h"
#include <cstdio>
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>
using namespace ms20;
static int fails = 0;
#define CHECK(c, m) do { bool k_ = (c); printf("  [%s] %s\n", k_ ? "PASS" : "FAIL", m); if (!k_) fails++; } while (0)
static int wavs(const char* dir, long* bytes) { int n = 0; *bytes = 0; if (DIR* d = opendir(dir)) { while (dirent* e = readdir(d)) { std::string s = e->d_name; if (s.size() > 4 && s.substr(s.size() - 4) == ".wav") { n++; struct stat st; stat((std::string(dir) + s).c_str(), &st); *bytes += st.st_size; } } closedir(d); } return n; }
int main()
{
  const char* dir = "/tmp/te_capture/"; mkdir(dir, 0755);
  { char c[300]; snprintf(c, sizeof(c), "rm -f %s*.wav", dir); if (system(c)) {} }
  Capture cap; cap.setFolder(dir); cap.setKeepAll(true);
  const double sr = 48000.0; float L[256], R[256];
  auto play = [&](double secs, bool sound, bool firstNote) {
    const int blocks = (int)(secs * sr / 256);
    for (int b = 0; b < blocks; b++) {
      for (int i = 0; i < 256; i++) { const float v = sound ? 0.5f * sinf((b * 256 + i) * 0.0577f) : 0.f; L[i] = v; R[i] = -v; }
      cap.block(L, R, 256, false, firstNote && b == 0, sr);
      if (b % 8 == 0) cap.idle();
    }
    cap.idle(); };
  printf("Takes in the app (no transport)\n");
  play(1.0, false, false);
  long bytes; CHECK(wavs(dir, &bytes) == 0 && cap.state() == 0, "nothing played: nothing recorded");
  play(1.0, true, true); play(3.0, false, false);
  CHECK(cap.takes() == 1 && wavs(dir, &bytes) == 1, "a note, then silence: one take");
  play(0.5, true, true); play(3.0, false, false);
  play(0.5, true, true); play(3.0, false, false);
  CHECK(cap.takes() == 3 && wavs(dir, &bytes) == 3, "three takes: all three kept (keep all)");
  FILE* f = fopen(cap.path().c_str(), "rb"); unsigned char h[44] = {0}; if (f) { if (fread(h, 1, 44, f)) {} fclose(f); }
  const unsigned dataBytes = h[40] | (h[41] << 8) | (h[42] << 16) | (h[43] << 24);
  const double secs = dataBytes / 6.0 / sr;
  printf("    last take: %s, %.2f s\n", cap.path().c_str() + strlen(dir), secs);
  CHECK(!memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4) && h[34] == 24 && h[22] == 2, "a stereo 24-bit WAV");
  CHECK(secs > 2.4 && secs < 2.7, "the take: the note and two seconds of silence after it");
  printf(fails ? "CAPTURE CHECKS FAILED (%d)\n" : "ALL CAPTURE CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
