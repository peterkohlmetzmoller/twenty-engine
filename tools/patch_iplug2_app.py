#!/usr/bin/env python3
# patch_iplug2_app.py <IPlugAPP_host.cpp>
#
# The standalone app "Twenty Engine" opens the MS-20ic itself (led_port.h: knobs, keys and LEDs). On Windows a
# MIDI port belongs to one program, so if the iC were also picked in the app's own MIDI settings (File >
# Preferences), RtMidi and the app would fight over it: whoever came second failed (RtMidi throws, inside the
# dialog), and with RtMidi first the iC's switch was lost. So:
#  - the iC is left out of the app's MIDI input and output lists (TE_IsControllerPort, TwentyEngine.cpp)
#  - an iC picked there before (the saved settings) is treated as "off"
#  - a port that fails to open (taken by another program, unplugged) is reported as not opened instead of an
#    exception escaping the settings dialog
import sys
p = sys.argv[1]
s = open(p).read()

def rep(old, new, what, count=1):
    global s
    n = s.count(old)
    if n != count:
        raise SystemExit('patch_iplug2_app: %s: found %d times (expected %d)' % (what, n, count))
    s = s.replace(old, new)

if 'TE_IsControllerPort' in s:
    print('already patched', p); sys.exit(0)

rep('''void IPlugAPPHost::ProbeMidiIO()
{''', '''bool TE_IsControllerPort(const char* name);   // Twenty Engine (TwentyEngine.cpp): the MS-20ic, which the app opens itself

void IPlugAPPHost::ProbeMidiIO()
{''', 'probe head')

rep('''    for (int i=0; i<nInputPorts; i++)
    {
      mMidiInputDevNames.push_back(mMidiIn->getPortName(i));
    }''', '''    for (int i=0; i<nInputPorts; i++)
    {
      if (TE_IsControllerPort(mMidiIn->getPortName(i).c_str())) continue;
      mMidiInputDevNames.push_back(mMidiIn->getPortName(i));
    }''', 'input list')

rep('''    for (int i=0; i<nOutputPorts; i++)
    {
      mMidiOutputDevNames.push_back(mMidiOut->getPortName(i));''', '''    for (int i=0; i<nOutputPorts; i++)
    {
      if (TE_IsControllerPort(mMidiOut->getPortName(i).c_str())) continue;
      mMidiOutputDevNames.push_back(mMidiOut->getPortName(i));''', 'output list')

rep('''bool IPlugAPPHost::SelectMIDIDevice(ERoute direction, const char* pPortName)
{
  int port = GetMIDIPortNumber(direction, pPortName);''', '''bool IPlugAPPHost::SelectMIDIDevice(ERoute direction, const char* pPortName)
{
  if (pPortName && TE_IsControllerPort(pPortName)) pPortName = OFF_TEXT;   // the app has the MS-20ic already
  int port = GetMIDIPortNumber(direction, pPortName);''', 'select head')

for d in ('In', 'Out'):
    rep('''        mMidi%s->openPort(port-1);
        return true;''' % d, '''        try { mMidi%s->openPort(port-1); } catch (RtMidiError&) { return false; }   // taken by another program, unplugged
        return true;''' % d, 'open %s (Windows)' % d)
    rep('''        mMidi%s->openPort(port-2);
        return true;''' % d, '''        try { mMidi%s->openPort(port-2); } catch (RtMidiError&) { return false; }
        return true;''' % d, 'open %s (Mac)' % d)

open(p, 'w').write(s)
print('patched', p)
