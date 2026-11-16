#!/usr/bin/env python3
# patch_iplug2.py <IPlugVST3_ProcessorBase.cpp>
#
# VST3 hosts hand MIDI CCs, channel pressure and pitch bend to a plugin as parameter changes (IMidiMapping): one
# queue per controller number, each with its points in the block. iPlug2 reads only the LAST point of each queue,
# one queue after the other. That loses messages and their order - fatal for the MS-20ic's three-part messages
# (NRPN-style CC 99, 98, 6: patch cables, the volume knob's power switch) when many arrive at once, as when the iC
# is switched on and reports everything.
#
# Here every point of every MIDI queue in the block is collected and put back in time order: by sample offset,
# then by its place in its own queue (the n-th 99 belongs with the n-th 98 and 6), then 99 before 98 before 6.
# TE_MidiBlock (TwentyEngine.cpp) sees the block first: it can ask for a clean "power on" to go first (the iC's
# power-on report recognised by its size) and keeps numbers for the plugin window.
import sys
p = sys.argv[1]
s = open(p).read()

old_head = """  IParameterChanges* paramChanges = data.inputParameterChanges;
  
  if (paramChanges)
  {
    int32 numParamsChanged = paramChanges->getParameterCount();
    """
new_head = """  IParameterChanges* paramChanges = data.inputParameterChanges;
  
  if (paramChanges)
  {
    int32 numParamsChanged = paramChanges->getParameterCount();
    // Twenty Engine: the block's MIDI points, put back in time order after the loop (see tools/patch_iplug2.py)
    struct TEPoint { int32 offset; int32 k; int rank; int ctrlr; int channel; double value; };
    static TEPoint tePts[2048]; int teN = 0, teMax = 0; bool teSame = false;
    """
assert s.count(old_head) == 1, 'head not found'
s = s.replace(old_head, new_head)

old_cc = """              else if (idx >= kMIDICCParamStartIdx)
              {
                int index = idx - kMIDICCParamStartIdx;
                int channel = index / kCountCtrlNumber;
                int ctrlr = index % kCountCtrlNumber;

                IMidiMsg msg;

                if (ctrlr == kAfterTouch)
                  msg.MakeChannelATMsg((int) (value * 127.), offsetSamples, channel);
                else if (ctrlr == kPitchBend)
                  msg.MakePitchWheelMsg((value * 2.)-1., channel, offsetSamples);
                else
                  msg.MakeControlChangeMsg((IMidiMsg::EControlChangeMsg) ctrlr, value, channel, offsetSamples);

                fromProcessor.Push(msg);
                ProcessMidiMsg(msg);
              }"""
new_cc = """              else if (idx >= kMIDICCParamStartIdx)
              {
                int index = idx - kMIDICCParamStartIdx;
                int channel = index / kCountCtrlNumber;
                int ctrlr = index % kCountCtrlNumber;
                if (numPoints > teMax) teMax = numPoints;
                int32 prevOff = -1;
                for (int32 pt = 0; pt < numPoints && teN < 2048; pt++)
                {
                  int32 o = 0; double v = 0.;
                  if (paramQueue->getPoint(pt, o, v) != kResultTrue) continue;
                  if (pt > 0 && o == prevOff) teSame = true;
                  prevOff = o;
                  const int rank = ctrlr == 99 ? 0 : (ctrlr == 98 ? 1 : (ctrlr == 6 ? 2 : 3));
                  tePts[teN++] = TEPoint{o, pt, rank, ctrlr, channel, v};
                }
              }"""
assert s.count(old_cc) == 1, 'cc block not found'
s = s.replace(old_cc, new_cc)

old_tail = """          }
        }
      }
    }
  }
}

void IPlugVST3ProcessorBase::ProcessAudio("""
new_tail = """          }
        }
      }
    }
    if (teN > 0)
    {
      std::stable_sort(tePts, tePts + teN, [](const TEPoint& a, const TEPoint& b) {
        if (a.offset != b.offset) return a.offset < b.offset;
        if (a.k != b.k) return a.k < b.k;
        return a.rank < b.rank;
      });
      int distinct = 0; { bool seen[128] = {}; for (int j = 0; j < teN; j++) if (tePts[j].channel == 0 && tePts[j].ctrlr < 128 && !seen[tePts[j].ctrlr]) { seen[tePts[j].ctrlr] = true; distinct++; } }
      if (TE_MidiBlock(teN, distinct, teMax, teSame))   // the iC's power-on report: a clean "power on" first
      {
        const int pc[3][2] = {{99, 6}, {98, 0}, {6, 127}};
        for (int j = 0; j < 3; j++)
        {
          IMidiMsg m; m.MakeControlChangeMsg((IMidiMsg::EControlChangeMsg) pc[j][0], pc[j][1] / 127., 0, tePts[0].offset);
          ProcessMidiMsg(m);
        }
      }
      for (int j = 0; j < teN; j++)
      {
        const TEPoint& t = tePts[j];
        IMidiMsg msg;
        if (t.ctrlr == kAfterTouch)
          msg.MakeChannelATMsg((int) (t.value * 127. + 0.5), t.offset, t.channel);
        else if (t.ctrlr == kPitchBend)
          msg.MakePitchWheelMsg((t.value * 2.)-1., t.channel, t.offset);
        else
          msg.MakeControlChangeMsg((IMidiMsg::EControlChangeMsg) t.ctrlr, t.value, t.channel, t.offset);
        fromProcessor.Push(msg);
        ProcessMidiMsg(msg);
      }
    }
  }
}

void IPlugVST3ProcessorBase::ProcessAudio("""
assert s.count(old_tail) == 1, 'tail not found'
s = s.replace(old_tail, new_tail)

anchor = 'void IPlugVST3ProcessorBase::ProcessParameterChanges('
s = s.replace(anchor, '#include <algorithm>\nbool TE_MidiBlock(int points, int distinctCCs, int maxPerCC, bool sameOffsets);   // TwentyEngine.cpp\n\n' + anchor, 1)
open(p, 'w').write(s)
print('patched', p)
