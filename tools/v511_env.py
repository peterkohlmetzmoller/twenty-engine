# v511_env.py - 1.1.1: snappier envelopes (EG 1, EG 2 and the per-voice EG 2 copies).
# Runs on the v51 patch:  python3 tools/v511_env.py patch/MS20_monopoly_v51.axp patch/MS20_monopoly_v51.axp
#  - The fast end of ATTACK, DECAY and RELEASE goes much further: fastest attack about 2 ms to 90 % (was about 8 ms),
#    fastest decay / release about 0.8 ms time constant (was about 5 ms). A cubic term on the per-tick coefficient:
#    it only matters near the fast end, the middle and slow half of each knob stay as they were.
#  - The per-voice amplitude de-click smoother: ~0.4 ms (was ~1 ms), so a fast attack is not rounded off again.
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()

def rep(old, new, what, n=1):
    global s
    c = s.count(old)
    if c != n: raise SystemExit(f'{what}: found {c} times')
    s = s.replace(old, new)

SNAP = lambda v, k: f'{v}+={v}*{v}*{v}*{k}.f;if({v}>0.6f){v}=0.6f;'
# per-voice EG 2 (MS20_Voices)
rep('float eac=0.00003f+(a_i*2.32e-10f)*0.35f;float edc=0.00002f+(d_i*2.32e-10f)*0.25f;float erc=0.00002f+(r_i*2.32e-10f)*0.25f;',
    'float eac=0.00003f+(a_i*2.32e-10f)*0.35f;float edc=0.00002f+(d_i*2.32e-10f)*0.25f;float erc=0.00002f+(r_i*2.32e-10f)*0.25f;'
    + SNAP('eac', 440) + SNAP('edc', 900) + SNAP('erc', 900) + '   /* 1.1.1: snappier fast end */', 'voice EG2')
# EG 1
rep('float ac=0.00003f+(ai*2.32e-10f)*0.35f;float rc=0.00002f+(ri*2.32e-10f)*0.25f;',
    'float ac=0.00003f+(ai*2.32e-10f)*0.35f;float rc=0.00002f+(ri*2.32e-10f)*0.25f;'
    + SNAP('ac', 440) + SNAP('rc', 900) + '   /* 1.1.1: snappier fast end */', 'EG1')
# EG 2 (filter, patch bay EG2 out)
rep('float ac=0.00003f+(ai*2.32e-10f)*0.35f;float dc=0.00002f+(di*2.32e-10f)*0.25f;float rc=0.00002f+(ri*2.32e-10f)*0.25f;',
    'float ac=0.00003f+(ai*2.32e-10f)*0.35f;float dc=0.00002f+(di*2.32e-10f)*0.25f;float rc=0.00002f+(ri*2.32e-10f)*0.25f;'
    + SNAP('ac', 440) + SNAP('dc', 900) + SNAP('rc', 900) + '   /* 1.1.1: snappier fast end */', 'EG2')
# de-click smoother
rep('vgn[v]+=0.02f*(ve[v]-vgn[v]); // ~1 ms de-click smoother on every voice amplitude',
    'vgn[v]+=0.05f*(ve[v]-vgn[v]); // ~0.4 ms de-click smoother on every voice amplitude (1.1.1: was ~1 ms)', 'smoother')
open(dst, 'w', encoding='utf-8').write(s)
print('v511_env: ok')
