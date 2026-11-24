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

  printf("The app quits in the middle of a take\n");
  auto header = [&](const std::string& p, double* s) { FILE* g = fopen(p.c_str(), "rb"); unsigned char hh[44] = {0}; long sz = 0;
    if (g) { if (fread(hh, 1, 44, g)) {} fseek(g, 0, SEEK_END); sz = ftell(g); fclose(g); }
    const unsigned d = hh[40] | (hh[41] << 8) | (hh[42] << 16) | (hh[43] << 24); *s = d / 6.0 / sr; return sz == (long)d + 44; };
  play(3.0, true, true);   // still sounding: the take is open
  CHECK(cap.state() == 1, "a take is being recorded");
  { double s3 = 0; header(cap.shownPath(), &s3); CHECK(s3 >= 1.9 && s3 <= 3.0, "while recording, the file's header follows every two seconds"); }
  const std::string open1 = cap.shownPath();
  cap.close();
  { double s4 = 0; CHECK(header(open1, &s4) && s4 > 2.9 && s4 < 3.1, "close(): the take is a whole WAV of what was played"); }
  { Capture c2; c2.setFolder(dir); c2.setKeepAll(true);
    for (int b = 0; b < 200; b++) { for (int i = 0; i < 256; i++) { L[i] = 0.3f; R[i] = -0.3f; } c2.block(L, R, 256, false, b == 0, sr); if (b % 8 == 0) c2.idle(); }
    c2.idle(); const std::string p2 = c2.shownPath();
    // ... and the app goes away without close(): the destructor still leaves a playable file
    struct Gone { Capture* c; std::string p; } g{&c2, p2};
    (void)g; }
  { int n = 0; double worst = 1; if (DIR* d = opendir(dir)) { while (dirent* e = readdir(d)) { std::string nm = e->d_name; if (nm.size() > 4 && nm.substr(nm.size() - 4) == ".wav") { double s5; if (header(std::string(dir) + nm, &s5)) n++; else worst = 0; } } closedir(d); }
    CHECK(worst == 1 && n == 5, "every file on disk has the right sizes in its header (5 takes)"); }
  printf(fails ? "CAPTURE CHECKS FAILED (%d)\n" : "ALL CAPTURE CHECKS PASSED (%d failed)\n", fails);
  return fails ? 1 : 0;
}
