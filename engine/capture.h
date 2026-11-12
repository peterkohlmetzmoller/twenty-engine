// capture.h - the plugin records what it plays, so the last take can be dragged into the DAW as audio.
//
//  - The transport starts: recording starts at once (so the audio lines up with the bar where playback started),
//    but it only counts once a note is played. A run without notes leaves nothing.
//  - Jamming with the transport stopped: recording starts at the first note and ends when the sound has been
//    silent for two seconds.
//  - The transport stops: recording goes on until the sound has died away (release, delay), then the file is done.
//  - A new recording replaces the last one - unless that one was dragged into a project: some DAWs use the file
//    where it lies (REAPER, Ableton), so a dragged capture is kept.
//  - The folder never grows beyond its limit (1 GB, about an hour): past it the oldest captures go first. The
//    newest is always kept, and so is any file still being written (by this or another copy of the plugin).
//  - What starts a recording is the plugin's business (TwentyEngine.cpp): keys played live, not a region's notes.
//
// Threads: the audio thread only writes samples and events into lock-free queues (block()); the main thread
// writes the WAV file (idle(), every ~20 ms) and is the only one that touches files or the state it shows.
#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

namespace ms20 {

class Capture {
public:
  // ---------------- audio thread ----------------
  // after the engines have played a block: L/R output, transport, whether a note started in this block
  void block(const float* L, const float* R, int n, bool running, bool noteOn, double sr)
  {
    if (n <= 0) return;
    float pk = 0.f;
    for (int i = 0; i < n; i++) { const float a = fabsf(L[i]), b = fabsf(R[i]); if (a > pk) pk = a; if (b > pk) pk = b; }
    const bool quiet = pk < 1e-4f;   // -80 dB
    const bool started = running && !mWasRunning;
    mWasRunning = running;
    switch (mSt)
    {
      case IDLE:
        if (started) { Ev(START, sr); mSt = PENDING; }
        else if (noteOn) { Ev(START, sr); Ev(CONFIRM, 0); mSt = running ? REC_RUN : REC_JAM; }
        break;
      case PENDING:
        if (noteOn) { Ev(CONFIRM, 0); mSt = REC_RUN; }
        else if (!running) { Ev(DISCARD, 0); mSt = IDLE; }
        break;
      case REC_RUN:
        if (!running) { mSt = TAIL; mQuiet = 0; mTail = 0; }
        break;
      case REC_JAM:
        if (started) { Ev(FINISH, 0); Ev(START, sr); mSt = PENDING; break; }
        mQuiet = quiet ? mQuiet + n : 0;
        if (mQuiet > (int64_t)(sr * 2.0)) { Ev(FINISH, 0); mSt = IDLE; }
        break;
      case TAIL:
        if (started) { Ev(FINISH, 0); Ev(START, sr); mSt = PENDING; break; }
        mQuiet = quiet ? mQuiet + n : 0; mTail += n;
        if (mQuiet > (int64_t)(sr * 1.0) || mTail > (int64_t)(sr * 15.0)) { Ev(FINISH, 0); mSt = IDLE; }
        break;
    }
    if (mSt == IDLE) return;
    if (mLen > (int64_t)(sr * 1800.0)) { Ev(FINISH, 0); mSt = IDLE; return; }   // half an hour at most
    for (int i = 0; i < n; i++) { mRing.push(L[i]); mRing.push(R[i]); }
    mPos += n; mLen += n;
  }

  // ---------------- main thread ----------------
  // the folder for the files (made if needed); call before the first idle()
  // limit: the most the folder may hold (bytes); the oldest captures go first when it is over
  void setFolder(const std::string& dir, uint64_t limit = 1024ull * 1024ull * 1024ull) { mDir = dir; mLimit = limit; mPruneDue = true; }
  // write what the audio thread recorded; handle its events in order
  void idle()
  {
    Event e;
    while (mEvents.peek(e))
    {
      Write(e.pos - mMainPos);   // the samples before the event
      mEvents.pop(e);
      switch (e.type)
      {
        case START:   Open(e.sr); break;
        case CONFIRM: if (mFile) { DropOld(); mShowing = 1; mPath = mCurPath; mDragged = false; mPeaks = mCurPeaks; } mConfirmed = true; break;
        case DISCARD: CloseFile(true); break;
        case FINISH:  if (mConfirmed) { CloseFile(false); mShowing = 2; mPruneDue = true; } else CloseFile(true); break;
      }
    }
    Write(-1);   // everything else that is there
    if (mPruneDue) { mPruneDue = false; Prune(); }
  }
  int state() const { return mShowing; }                 // 0 nothing, 1 recording, 2 ready to drag
  double seconds() const { return mShowSecs; }
  const std::string& path() const { return mPath; }
  const std::vector<float>& peaks() const { return mPeaks; }   // one per 1024 frames, 0..1
  void dragged() { mDragged = true; }                     // in a project now: never deleted

private:
  enum { IDLE, PENDING, REC_RUN, REC_JAM, TAIL };
  enum { START, CONFIRM, DISCARD, FINISH };
  struct Event { int type; int64_t pos; double sr; };

  // single producer / single consumer queues
  template <typename T, int N> struct Q {
    T buf[N]; std::atomic<int> h{0}, t{0};
    bool push(const T& v) { const int a = h.load(std::memory_order_relaxed), b = (a + 1) % N; if (b == t.load(std::memory_order_acquire)) return false; buf[a] = v; h.store(b, std::memory_order_release); return true; }
    bool peek(T& v) const { const int a = t.load(std::memory_order_relaxed); if (a == h.load(std::memory_order_acquire)) return false; v = buf[a]; return true; }
    bool pop(T& v) { if (!peek(v)) return false; t.store((t.load(std::memory_order_relaxed) + 1) % N, std::memory_order_release); return true; }
    int size() const { const int a = h.load(std::memory_order_acquire), b = t.load(std::memory_order_relaxed); return (a - b + N) % N; }
  };
  struct Ring {   // interleaved stereo samples, ~10 s at 48 kHz
    static const int N = 1 << 20;
    std::vector<float> buf = std::vector<float>(N);
    std::atomic<int> h{0}, t{0};
    void push(float v) { const int a = h.load(std::memory_order_relaxed), b = (a + 1) & (N - 1); if (b == t.load(std::memory_order_acquire)) return; buf[a] = v; h.store(b, std::memory_order_release); }
    int avail() const { return (h.load(std::memory_order_acquire) - t.load(std::memory_order_relaxed)) & (N - 1); }
    float pop() { const int a = t.load(std::memory_order_relaxed); const float v = buf[a]; t.store((a + 1) & (N - 1), std::memory_order_release); return v; }
  };

  void Ev(int type, double sr) { mEvents.push({type, mPos, sr}); if (type == START) mLen = 0; }

  // frames: how many to write (-1 = all there are); they go to the open file, or are skipped
  void Write(int64_t frames)
  {
    int64_t have = mRing.avail() / 2;
    if (frames >= 0 && frames < have) have = frames;
    uint8_t out[3 * 2 * 256]; int o = 0;
    for (int64_t i = 0; i < have; i++)
    {
      const float l = mRing.pop(), r = mRing.pop();
      mMainPos++;
      if (!mFile) continue;
      Put24(out + o, l); Put24(out + o + 3, r); o += 6;
      if (o >= (int)sizeof(out)) { fwrite(out, 1, o, mFile); o = 0; }
      mFrames++;
      const float a = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r); if (a > mPeakMax) mPeakMax = a;
      if (++mPeakPos >= 1024) { const float v = mPeakMax > 1.f ? 1.f : mPeakMax; mCurPeaks.push_back(v); if (mConfirmed) mPeaks.push_back(v); mPeakPos = 0; mPeakMax = 0.f; }
    }
    if (o && mFile) fwrite(out, 1, o, mFile);
    if (mFile && mConfirmed && mSr > 0.0) mShowSecs = mFrames / mSr;
  }
  static void Put24(uint8_t* p, float v)
  {
    if (v > 1.f) v = 1.f;
    if (v < -1.f) v = -1.f;
    const int32_t s = (int32_t)lrintf(v * 8388607.f);
    p[0] = (uint8_t)(s & 0xFF); p[1] = (uint8_t)((s >> 8) & 0xFF); p[2] = (uint8_t)((s >> 16) & 0xFF);
  }
  void Open(double sr)
  {
    CloseFile(true);
    if (mDir.empty()) return;
    char name[64]; const time_t t = time(nullptr); struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    strftime(name, sizeof(name), "Twenty Engine %Y-%m-%d %H.%M.%S", &lt);
    // a name of its own: another copy of the plugin may start a file in the same second
    for (int i = 1; i <= 50 && !mFile; i++)
    {
      char suf[16] = ""; if (i > 1) snprintf(suf, sizeof(suf), " %d", i);
      mCurPath = mDir + name + suf + ".wav";
      mFile = CreateNew(mCurPath.c_str());
    }
    mSr = sr; mFrames = 0; mConfirmed = false; mCurPeaks.clear(); mPeakPos = 0; mPeakMax = 0.f;
    if (mFile) Header(0);
  }
  void Header(uint32_t frames)
  {
    const uint32_t sr = (uint32_t)(mSr + 0.5), data = frames * 6, riff = 36 + data, br = sr * 6;
    const uint16_t fmt = 1, ch = 2, ba = 6, bits = 24; const uint32_t fl = 16;
    fseek(mFile, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, mFile); fwrite(&riff, 4, 1, mFile); fwrite("WAVEfmt ", 1, 8, mFile);
    fwrite(&fl, 4, 1, mFile); fwrite(&fmt, 2, 1, mFile); fwrite(&ch, 2, 1, mFile); fwrite(&sr, 4, 1, mFile);
    fwrite(&br, 4, 1, mFile); fwrite(&ba, 2, 1, mFile); fwrite(&bits, 2, 1, mFile);
    fwrite("data", 1, 4, mFile); fwrite(&data, 4, 1, mFile);
  }
  void CloseFile(bool remove_)
  {
    if (!mFile) return;
    if (!remove_) { Header((uint32_t)mFrames); fseek(mFile, 0, SEEK_END); }
    fclose(mFile); mFile = nullptr;
    if (remove_) remove(mCurPath.c_str());
    mConfirmed = false;
  }
  static FILE* CreateNew(const char* path)   // only if there is no such file yet
  {
#ifdef _WIN32
    const int fd = _open(path, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
    return fd >= 0 ? _fdopen(fd, "wb") : nullptr;
#else
    const int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    return fd >= 0 ? fdopen(fd, "wb") : nullptr;
#endif
  }
  // keep the folder under its limit: the captures sorted by name are sorted by time; the oldest go first
  void Prune()
  {
    struct F { std::string name; uint64_t size; time_t mtime; };
    std::vector<F> fs; uint64_t total = 0;
    auto mine = [](const std::string& n) { return n.size() > 18 && n.compare(0, 14, "Twenty Engine ") == 0 && n.compare(n.size() - 4, 4, ".wav") == 0; };
#ifdef _WIN32
    struct __finddata64_t fd; const intptr_t h = _findfirst64((mDir + "*.wav").c_str(), &fd);
    if (h != -1) { do { if (!(fd.attrib & _A_SUBDIR) && mine(fd.name)) fs.push_back({fd.name, (uint64_t)fd.size, (time_t)fd.time_write}); } while (_findnext64(h, &fd) == 0); _findclose(h); }
#else
    if (DIR* d = opendir(mDir.c_str()))
    {
      while (struct dirent* e = readdir(d))
      {
        if (!mine(e->d_name)) continue;
        struct stat st; if (stat((mDir + e->d_name).c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        fs.push_back({e->d_name, (uint64_t)st.st_size, st.st_mtime});
      }
      closedir(d);
    }
#endif
    for (const F& f : fs) total += f.size;
    if (total <= mLimit) return;
    std::sort(fs.begin(), fs.end(), [](const F& a, const F& b) { return a.name < b.name; });
    const time_t now = time(nullptr);
    for (size_t i = 0; i + 1 < fs.size() && total > mLimit; i++)   // never the newest
    {
      const std::string p = mDir + fs[i].name;
      if (p == mPath || p == mCurPath) continue;          // the one shown, the one being written
      if (now - fs[i].mtime < 120) continue;                // still being written (another copy)
      if (remove(p.c_str()) == 0) total -= fs[i].size;
    }
  }
  void DropOld() { if (!mPath.empty() && !mDragged && mPath != mCurPath) remove(mPath.c_str()); }

  // audio thread
  int mSt = IDLE; bool mWasRunning = false; int64_t mQuiet = 0, mTail = 0, mPos = 0, mLen = 0;
  Ring mRing; Q<Event, 64> mEvents;
  // main thread
  std::string mDir, mCurPath, mPath; uint64_t mLimit = 0; bool mPruneDue = false;
  FILE* mFile = nullptr; double mSr = 0.0; int64_t mFrames = 0, mMainPos = 0;
  bool mConfirmed = false, mDragged = false;
  int mShowing = 0; double mShowSecs = 0.0;
  std::vector<float> mPeaks, mCurPeaks; int mPeakPos = 0; float mPeakMax = 0.f;
};

}  // namespace ms20
