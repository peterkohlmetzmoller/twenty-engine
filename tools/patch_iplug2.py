#!/usr/bin/env python3
# Small patch to the pinned iPlug2 commit: pass the host's "recording" state to the plugin
# (ITimeInfo::mTransportIsRecording, GetTransportIsRecording()). Used by performance recall (engine/recall.h).
# AU: HostCallback_GetTransportState2 (outIsRecording). VST3: ProcessContext::kRecording.
# Each edit must match exactly once, or the build stops.
import sys, os
root = sys.argv[1] if len(sys.argv) > 1 else 'iPlug2'
edits = [
 ('IPlug/IPlugStructs.h',
  '  bool mTransportLoopEnabled = false;\n',
  '  bool mTransportLoopEnabled = false;\n  bool mTransportIsRecording = false;   /* Twenty Engine patch */\n'),
 ('IPlug/IPlugProcessor.h',
  '  bool GetTransportIsRunning() const { return mTimeInfo.mTransportIsRunning; }\n',
  '  bool GetTransportIsRunning() const { return mTimeInfo.mTransportIsRunning; }\n'
  '  /** @return \\c true if the host is recording (Twenty Engine patch) */\n'
  '  bool GetTransportIsRecording() const { return mTimeInfo.mTransportIsRecording; }\n'),
 ('IPlug/AUv2/IPlugAU.cpp',
  '    timeInfo.mTransportIsRunning = playing;\n    timeInfo.mTransportLoopEnabled = looping;\n  }\n',
  '    timeInfo.mTransportIsRunning = playing;\n    timeInfo.mTransportLoopEnabled = looping;\n  }\n'
  '  if (mHostCallbacks.transportStateProc2)   /* Twenty Engine patch: recording state */\n'
  '  {\n'
  '    Boolean p2 = false, r2 = false, c2 = false, cy2 = false; Float64 s2 = 0.0, cs2 = 0.0, ce2 = 0.0;\n'
  '    if (mHostCallbacks.transportStateProc2(mHostCallbacks.hostUserData, &p2, &r2, &c2, &s2, &cy2, &cs2, &ce2) == noErr)\n'
  '      timeInfo.mTransportIsRecording = r2;\n'
  '  }\n'),
 ('IPlug/VST3/IPlugVST3_ProcessorBase.cpp',
  '  timeInfo.mTransportIsRunning = mProcessContext.state & ProcessContext::kPlaying;\n',
  '  timeInfo.mTransportIsRunning = mProcessContext.state & ProcessContext::kPlaying;\n'
  '  timeInfo.mTransportIsRecording = mProcessContext.state & ProcessContext::kRecording;   /* Twenty Engine patch */\n'),
]
for f, old, new in edits:
    p = os.path.join(root, f); s = open(p, encoding='utf-8').read()
    if s.count(old) != 1: raise SystemExit('patch_iplug2: no unique match in ' + f)
    open(p, 'w', encoding='utf-8').write(s.replace(old, new))
    print('patched', f)
