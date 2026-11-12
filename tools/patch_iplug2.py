#!/usr/bin/env python3
# patch_iplug2.py <IPlugVST3_ProcessorBase.cpp>
# VST3 hosts deliver MIDI CC, channel pressure and pitch bend as parameter changes (IMidiMapping). iPlug2 reads only the
# last point of each parameter's queue in a block; a host that sends several points (the MS-20ic's switch pressed and
# let go, or two changes in one block) loses all but one. Here every point becomes a MIDI message, in order, and
# channel pressure is rounded like the CCs. TE_SwitchDiag (TwentyEngine.cpp) sees each channel-pressure point.
import sys
p = sys.argv[1]
s = open(p).read()
old = """                int index = idx - kMIDICCParamStartIdx;
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
                ProcessMidiMsg(msg);"""
new = """                int index = idx - kMIDICCParamStartIdx;
                int channel = index / kCountCtrlNumber;
                int ctrlr = index % kCountCtrlNumber;

                for (int32 pt = 0; pt < numPoints; pt++)   // every point, in order (Twenty Engine patch)
                {
                  int32 ptOffset = 0; double ptValue = 0.;
                  if (paramQueue->getPoint(pt, ptOffset, ptValue) != kResultTrue) continue;
                  IMidiMsg msg;

                  if (ctrlr == kAfterTouch)
                  {
                    TE_SwitchDiag(numPoints, pt, ptOffset, ptValue);
                    msg.MakeChannelATMsg((int) (ptValue * 127. + 0.5), ptOffset, channel);
                  }
                  else if (ctrlr == kPitchBend)
                    msg.MakePitchWheelMsg((ptValue * 2.)-1., channel, ptOffset);
                  else
                    msg.MakeControlChangeMsg((IMidiMsg::EControlChangeMsg) ctrlr, ptValue, channel, ptOffset);

                  fromProcessor.Push(msg);
                  ProcessMidiMsg(msg);
                }"""
assert s.count(old) == 1, 'ProcessParameterChanges: the MIDI CC block was not found'
s = s.replace(old, new)
anchor = 'void IPlugVST3ProcessorBase::ProcessParameterChanges('
s = s.replace(anchor, 'void TE_SwitchDiag(int numPoints, int point, int offset, double value);   // TwentyEngine.cpp\n\n' + anchor, 1)
open(p, 'w').write(s)
print('patched', p)
