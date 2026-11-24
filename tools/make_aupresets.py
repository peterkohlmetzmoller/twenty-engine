#!/usr/bin/env python3
"""Twenty Engine UI: the 37 sounds of the preset menu as .aupreset files, for hosts that only list preset files
(Ableton Live, Push). Each file is the same marker as the built-in preset: 'TEMS' + the memory slot, so picking it
recalls that memory (MS20MEM.BIN), as switch + key does. The names and the order come from TwentyEngine.cpp.

usage: make_aupresets.py <TwentyEngine.cpp> <config.h> <out dir>"""
import os, plistlib, re, struct, sys

src, cfg, out = sys.argv[1:4]
s = open(src).read()
names = re.findall(r'"([^"]+)"', s[s.index('kFactory[36] = {'):s.index('};', s.index('kFactory[36] = {'))])
assert len(names) == 36, len(names)
ver = int(re.search(r'#define PLUG_VERSION_HEX (0x[0-9A-Fa-f]+)', open(cfg).read()).group(1), 16)
four = lambda t: struct.unpack('>I', t.encode())[0]
os.makedirs(out, exist_ok=True)
MAGIC = 0x54454D53   # 'TEMS', TwentyEngine.h kMemPresetMagic
for idx in range(37):   # the panel first, as in the host's preset menu
    slot = 36 if idx == 0 else idx - 1
    name = 'Panel (the knobs as they are)' if slot == 36 else '%02d %s' % (slot + 1, names[slot])
    fname = '00 Panel' if slot == 36 else name
    d = {'version': ver, 'type': four('aumu'), 'subtype': four('Tw2U'), 'manufacturer': four('Pkmo'),
         'name': name, 'data': struct.pack('<ii', MAGIC, slot)}
    with open(os.path.join(out, fname + '.aupreset'), 'wb') as f:
        plistlib.dump(d, f, fmt=plistlib.FMT_XML)
print('%d presets in %s' % (len(os.listdir(out)), out))
