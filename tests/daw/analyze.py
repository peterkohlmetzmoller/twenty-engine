#!/usr/bin/env python3
# analyze.py <folder>: checks the renders reaper_test.lua made. Prints a table and exits 1 on a failure.
#  - every render: sound, no clipping, no DC, no dropouts (silent gaps while notes are held)
#  - cc74 / auto: the sweep really opens the filter (brightness rises much more than in the plain render)
#  - state: knob values come back after the plugin's state is saved and loaded
import sys, os, wave, struct, math, re

d = sys.argv[1]
log = open(os.path.join(d, 'reaper_log.txt')).read() if os.path.exists(os.path.join(d, 'reaper_log.txt')) else ''
fails = []

def read(path):
    w = wave.open(path)
    n, ch, sw, sr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(n)
    if sw == 3:
        mono = []
        step = 3 * ch
        for i in range(0, len(raw) - step + 1, step):
            s = 0
            for c in range(ch):
                b = raw[i + 3 * c:i + 3 * c + 3]
                v = int.from_bytes(b, 'little', signed=True)
                s += v
            mono.append(s / ch / 8388608.0)
    elif sw == 2:
        a = struct.unpack('<%dh' % (len(raw) // 2), raw)
        mono = [sum(a[i:i + ch]) / ch / 32768.0 for i in range(0, len(a), ch)]
    elif sw == 4:
        a = struct.unpack('<%df' % (len(raw) // 4), raw)
        mono = [sum(a[i:i + ch]) / ch for i in range(0, len(a), ch)]
    return mono, sr

def seg(x, sr, t0, t1):
    return x[int(t0 * sr):int(t1 * sr)]

def rms(x):
    return math.sqrt(sum(v * v for v in x) / max(1, len(x)))

def bright(x):   # energy of the first difference relative to the signal: higher = brighter
    e = sum(v * v for v in x)
    if e <= 1e-12: return 0.0
    return sum((x[i] - x[i - 1]) ** 2 for i in range(1, len(x))) / e

res = {}
for f in sorted(os.listdir(d)):
    if not f.endswith('.wav'): continue
    name = f[:-4]
    x, sr = read(os.path.join(d, f))
    peak = max(abs(v) for v in x) if x else 0
    clip = sum(1 for v in x if abs(v) > 0.999)
    dc = abs(sum(x) / max(1, len(x)))
    # dropouts: 10 ms windows that are silent while notes are held (0.6-2.4 s and 5.1-8.4 s)
    gaps = 0
    for (a, b) in ((0.6, 2.4), (5.1, 8.4)):
        s = seg(x, sr, a, b)
        for i in range(0, len(s) - 480, 480):
            if rms(s[i:i + 480]) < 1e-4: gaps += 1
    early, late = bright(seg(x, sr, 0.8, 2.2)), bright(seg(x, sr, 7.6, 8.4))   # chord (sweep nearly closed) / end (open)
    res[name] = dict(peak=peak, clip=clip, dc=dc, gaps=gaps, early=early, late=late, rms=rms(x))
    ok = peak > 0.01 and clip < 50 and dc < 0.02 and gaps == 0
    print('%-22s peak %6.1f dB  rms %6.1f dB  clip %4d  dc %.4f  gaps %3d  bright %.4f -> %.4f  %s' % (
        name, 20 * math.log10(peak + 1e-9), 20 * math.log10(res[name]['rms'] + 1e-9), clip, dc, gaps, early, late, 'ok' if ok else 'FAIL'))
    if not ok: fails.append(name + ': level/clip/dc/dropout')

for name in list(res):
    for kind in ('cc74', 'auto'):
        if name.endswith('_' + kind):
            base = res.get(name[:-len(kind)] + 'notes')
            r = res[name]
            if not base: continue
            gain = base['early'] / max(r['early'], 1e-9)    # early in the sweep the filter is nearly closed: much darker
            # required: MIDI CC moves the instrument's knobs (no controller attached), automation moves Studio's
            need = (name.startswith('inst') and kind == 'cc74') or (name.startswith('studio') and kind == 'auto')
            ok = gain > 1.5
            print('%-22s the sweep moves the filter: chord %.1fx darker than the plain render, end %.2fx  %s' % (
                name, gain, r['late'] / max(base['late'], 1e-9), ('ok' if ok else 'FAIL') if need else '(info)'))
            if need and not ok: fails.append(name + ': sweep had no effect')

for m in re.finditer(r'^STATE (\S+) ([\d.-]+) ([\d.-]+)', log, re.M):
    a, b = float(m.group(2)), float(m.group(3))
    ok = abs(a - 0.123) < 0.01 and (b < 0 or abs(b - 0.789) < 0.01)
    print('%-22s state saved and loaded: %.3f %.3f  %s' % (m.group(1), a, b, 'ok' if ok else 'FAIL'))
    if not ok: fails.append(m.group(1) + ': state')

for m in re.finditer(r'^(FAIL|ERROR) (.*)$', log, re.M):
    fails.append(m.group(0))
if 'DONE' not in log: fails.append('the REAPER script did not finish')
if not res: fails.append('no renders')

print('\n' + ('ALL DAW CHECKS PASSED' if not fails else 'FAILED:\n  ' + '\n  '.join(fails)))
sys.exit(1 if fails else 0)
