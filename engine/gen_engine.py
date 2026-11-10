# gen_engine.py - turns the Ksoloti patch (.axp) into a portable C++ engine (engine_gen.h).
# Every embedded object becomes a class with exactly the code from the patch; the wiring (nets),
# execution order (Ksoloti sorts objects top-to-bottom, left-to-right), parameter defaults and
# the editor's CC assignments are reproduced the way the Ksoloti code generator does them.
# Usage: python3 gen_engine.py MS20_monopoly_vXX.axp engine_gen.h
import sys, re, xml.etree.ElementTree as ET

axp, out = sys.argv[1], sys.argv[2]
root = ET.parse(axp).getroot()

def esc(n):            # Ksoloti charEscape for instance names: '_' doubled
    return n.replace('_', '__')

# Objects that only make sense inside the hardware are left out of the plugin; their outlets read 0.
# USB_Bridge: in the plugin the iC is connected to the computer directly (no forwarding, no legacy mode).
EXCLUDE = {'USB_Bridge'}
objs = []              # (y, x, name, kind, data)
for e in root:
    if e.tag not in ('patchobj', 'obj'):
        continue
    if e.get('name') in EXCLUDE:
        continue
    objs.append((int(e.get('y')), int(e.get('x')), e.get('name'), e.tag, e))
objs.sort(key=lambda o: (o[0], o[1]))          # Ksoloti execution order

STOCK = {
    'math/+':           {'in': [('frac32', 'in1'), ('frac32', 'in2')], 'out': [('frac32', 'out')], 'k': 'outlet_out=inlet_in1+inlet_in2;'},
    'audio/out stereo': {'in': [('frac32buffer', 'left'), ('frac32buffer', 'right')], 'out': [], 'k': None},
    'audio/in left':    {'in': [], 'out': [('frac32buffer', 'wave')], 'k': None},
}

# ---------- port tables ----------
ports = {}            # name -> (inlets, outlets)
for y, x, n, kind, e in objs:
    if kind == 'patchobj':
        d = e.find('object')
        ins = [(c.tag, c.get('name')) for c in d.find('inlets')]
        outs = [(c.tag, c.get('name')) for c in d.find('outlets')]
        ports[n] = (ins, outs)
    else:
        t = e.get('type')
        if t in STOCK:
            ports[n] = (STOCK[t]['in'], STOCK[t]['out'])
        elif t.startswith('disp/'):
            ports[n] = ([('int32', 'in')], [])   # displays: ignored
        else:
            raise SystemExit('unknown stock object ' + t)

def isbuf(t): return t.endswith('buffer')

# ---------- nets ----------
nets = []             # (cname, isbuffer, source(obj,outlet), [dests])
for i, ne in enumerate(root.find('nets')):
    src = ne.find('source'); dests = [(d.get('obj'), d.get('inlet')) for d in ne.findall('dest')]
    so, sp = src.get('obj'), src.get('outlet')
    dests = [d for d in dests if d[0] in ports]
    if so not in ports:
        nets.append((f'net{i}', False, (so, sp), dests)); continue   # source left out: stays 0
    typ = dict((p, t) for t, p in ports[so][1])[sp]
    nets.append((f'net{i}', isbuf(typ), (so, sp), dests))
src_net = {n[2]: n for n in nets}
dst_net = {}
for n in nets:
    for d in n[3]:
        dst_net[d] = n

# ---------- parameters ----------
params = []           # (index_name, obj, pname, ptype, default, midicc, hradio_max)
for y, x, n, kind, e in objs:
    if kind != 'patchobj':
        continue
    d = e.find('object')
    defs = {c.get('name'): c for c in d.find('params')}
    inst = {c.get('name'): c for c in e.find('params')} if e.find('params') is not None else {}
    for pn, pd in defs.items():
        ty = pd.tag
        pi = inst.get(pn)
        v = float(pi.get('value')) if pi is not None and pi.get('value') is not None else 0.0
        if ty.startswith('frac32'):
            dv = int(round(v * (1 << 21)))
        else:
            dv = int(v)
        mx = pd.find('MaxValue')
        hmax = int(mx.get('i')) if mx is not None else None
        cc = pi.get('MidiCC') if pi is not None else None
        params.append((f'PARAM_INDEX_{esc(n)}_{pn}', n, pn, ty, dv, cc, hmax))

def cc_value(ty, hmax):
    if ty.startswith('frac32.u'):
        return '(data2!=127)?data2<<20:0x07FFFFFF'
    if ty.startswith('frac32.s'):
        return '(data2!=127)?(data2-64)<<21:0x07FFFFFF'
    if ty.startswith('int32.hradio'):
        return f'(data2<{hmax})?data2:{hmax - 1}'
    if ty.startswith('bool32'):
        return '(data2>0)?1:0'
    raise SystemExit('no CC mapping for ' + ty)

# the Ksoloti's size/placement attributes (optimize("Os"), section(".sram3...")): meaningless in the plugin
SIZEOPT = re.compile(r'__attribute__\s*\(\(\s*noinline\s*,\s*optimize\s*\(\s*"Os"\s*\)\s*(,\s*section\s*\(\s*"[^"]*"\s*\)\s*)?\)\)\s*')
# HQ: what the plugin can afford and the Ksoloti cannot (each must match exactly once, or the build stops)
HQ = {
 'MS20_Voices': [
  ('return o;   /* v48b: triangle corner correction off (CPU) */', '/* plugin HQ: triangle corner correction on */'),
  # v50d fix: pblm fell through to the "near 1" formula when t landed exactly on dt (or the guard product underflowed),
  # returning values up to ~-650 for one sample = the random clicks on triangle/sine. Only the HQ build calls pblm.
  ('if(t<dt){t=t*rdt-1.f;return -0.33333333f*t*t*t;}t=(t-1.f)*rdt+1.f;return 0.33333333f*t*t*t;}',
   'if(t<dt){t=t*rdt-1.f;return -0.33333333f*t*t*t;}if(t>1.f-dt){t=(t-1.f)*rdt+1.f;return 0.33333333f*t*t*t;}return 0.f;} /* v50d: no fall-through */'),
  # HQ: poly oscillators at MS20_OSF x 48 kHz (2 = 96 kHz), back to 48 kHz through the filter's half-band FIR
  ('float ap_s[12],ph_fb,ph_a,ph_da;bool phwas;',
   'float ap_s[12],ph_fb,ph_a,ph_da;bool phwas;\n'
   'static_assert(MS20_OSF==1||MS20_OSF==2,"MS20_OSF: 1 or 2");\n'
   '/* HQ: half-band decimators for the oversampled oscillators (same FIR as the filter) */\n'
   'float dr1[28],do1[16],dr2[28],do2[16];int di1,dj1,di2,dj2;\n'
   'inline float* hbp(float*b,int&ix,int n,float v){ix=(ix==0)?n-1:ix-1;b[ix]=v;b[ix+n]=v;return b+ix;}\n'
   'inline float hbs(const float*b){return 0.001819877f*(b[0]+b[13])-0.005222563f*(b[1]+b[12])+0.012064004f*(b[2]+b[11])-0.024375518f*(b[3]+b[10])\n'
   ' +0.046728258f*(b[4]+b[9])-0.095109546f*(b[5]+b[8])+0.314496715f*(b[6]+b[7]);}\n'
   '/* HQ: analog drift - fixed per-voice character (attack, decay, release, level), scaled by g_drift */\n'
   'float vrr[NV][4];'),
  ('ph_fb=ph_a=ph_da=0.f;phwas=false;',
   'ph_fb=ph_a=ph_da=0.f;phwas=false;\n'
   'for(int k=0;k<28;k++){dr1[k]=dr2[k]=0.f;}for(int k=0;k<16;k++){do1[k]=do2[k]=0.f;}di1=dj1=di2=dj2=0;\n'
   '{uint32_t r=97531U;for(int i=0;i<NV;i++)for(int k=0;k<4;k++){r=r*196314165U+907633515U;vrr[i][k]=(int32_t)r*4.6566e-10f;}}'),
  ('inc1[v]=f1*2.3283e-10f;if(inc1[v]>0.48f)', 'inc1[v]=f1*(2.3283e-10f/MS20_OSF);if(inc1[v]>0.48f)'),
  ('inc2[v]=f2*2.3283e-10f;if(inc2[v]>0.48f)', 'inc2[v]=f2*(2.3283e-10f/MS20_OSF);if(inc2[v]>0.48f)'),
  # HQ drift: detune x1..2, slow pitch wander x1..4 (+-0.8 .. +-3.2 cents), envelope times +-15 %, level +-0.8 dB
  ('(int32_t)(vp[v]+dtn[v]+drf[v])', '(int32_t)(vp[v]+dtn[v]*(1.f+g_drift)+drf[v]*(1.f+3.f*g_drift))'),
  ('(int32_t)(vp[v]+dtn2[v]+drf[v])', '(int32_t)(vp[v]+dtn2[v]*(1.f+g_drift)+drf[v]*(1.f+3.f*g_drift))'),
  ('ve[v]+=(1.05f-ve[v])*eac;', 've[v]+=(1.05f-ve[v])*eac*(1.f+0.15f*g_drift*vrr[v][0]);'),
  ('ve[v]+=(sus-ve[v])*edc;', 've[v]+=(sus-ve[v])*edc*(1.f+0.15f*g_drift*vrr[v][1]);'),
  ('ve[v]+=(0.f-ve[v])*erc-0.00001f;', 've[v]+=(0.f-ve[v])*erc*(1.f+0.15f*g_drift*vrr[v][2])-0.00001f;'),
  ('float s1=0.f,s2=0.f;\nfor(int v=0;v<NV;v++){\n vgn[v]+=0.02f*(ve[v]-vgn[v]); // ~1 ms de-click smoother on every voice amplitude\n if(vgn[v]>0.00005f){',
   'float s1=0.f,s2=0.f;float s1o[2]={0.f,0.f},s2o[2]={0.f,0.f};\nfor(int v=0;v<NV;v++){\n vgn[v]+=0.02f*(ve[v]-vgn[v]); // ~1 ms de-click smoother on every voice amplitude\n'
   ' float lv=vgn[v]*0.15f*(1.f+0.1f*g_drift*vrr[v][3]);   /* HQ drift: voice level */\n'
   ' if(vgn[v]>0.00005f)for(int os=0;os<MS20_OSF;os++){   /* HQ: oscillators at MS20_OSF x 48 kHz */'),
  ('s1+=o*(vgn[v]*0.15f);', 's1o[os]+=o*lv;'),
  ('s2+=o2*(vgn[v]*0.15f);', 's2o[os]+=o2*lv;'),
  ('if(phon){                                    // 12 allpass',
   'if(MS20_OSF==2){   /* HQ: half-band down to 48 kHz */\n'
   ' s1=hbs(hbp(dr1,di1,14,s1o[0]))+0.5f*do1[dj1+6];hbp(do1,dj1,8,s1o[1]);\n'
   ' s2=hbs(hbp(dr2,di2,14,s2o[0]))+0.5f*do2[dj2+6];hbp(do2,dj2,8,s2o[1]);\n'
   '}else{s1=s1o[0];s2=s2o[0];}\n'
   'if(phon){                                    // 12 allpass'),
 ],
 'Filter_Macro': [
  ('float gha=fha*3.65e-10f;if(gha>0.72f)gha=0.72f;if(gha<0.0004f)gha=0.0004f;   /* arp filter: 48 kHz */',
   'float gha=fha*1.825e-10f;if(gha>0.36f)gha=0.36f;if(gha<0.0002f)gha=0.0002f;   /* plugin HQ: arp filter at 96 kHz too */'),
  ('float gla=fla*3.65e-10f;if(gla>0.75f)gla=0.75f;if(gla<0.0008f)gla=0.0008f;',
   'float gla=fla*1.825e-10f;if(gla>0.375f)gla=0.375f;if(gla<0.0004f)gla=0.0004f;'),
  (' a1ha+=2.f*dcc[6];a2ha+=2.f*dcc[7];a3ha+=2.f*dcc[8];a1la+=2.f*dcc[9];a2la+=2.f*dcc[10];a3la+=2.f*dcc[11];   /* 16 steps per block */',
   ' a1ha+=dcc[6];a2ha+=dcc[7];a3ha+=dcc[8];a1la+=dcc[9];a2la+=dcc[10];a3la+=dcc[11];'),
  ('float ya=stA(v0a);   /* arp filter at 48 kHz (saves CPU in arp modes) */',
   'const float*uy=hbp(rxa,ixa,14,v0a);float ya0=stA(2.f*hbs(uy));float ya1=stA(uy[6]);\nfloat ya=hbs(hbp(rea,iea,14,ya0))+0.5f*roa[ioa+6];hbp(roa,ioa,8,ya1);'),
 ],
}
def hq(name, parts):
    """apply the HQ patches to whichever code tab holds them (declaration, k-rate, s-rate)"""
    for a, b in HQ.get(name, []):
        hits = [k for k in parts if parts[k].count(a) == 1]
        assert len(hits) == 1 and sum(parts[k].count(a) for k in parts) == 1, ('HQ patch does not match', name, a[:60])
        parts[hits[0]] = parts[hits[0]].replace(a, b)
    return parts
SECTION = re.compile(r'__attribute__\s*\(\(\s*section\s*\(\s*"\.sdram"\s*\)\s*\)\)')

L = ['// GENERATED by gen_engine.py from ' + axp.split('/')[-1] + ' - do not edit by hand',
     '#pragma once', '#include "ksoloti_host.h"', 'namespace ms20 {', '']
L.append(f'static const int NPARAMS = {len(params)};')
for i, p in enumerate(params):
    L.append(f'static const int {p[0]} = {i};')
L.append('struct rootc { ParameterExchange_t PExch[NPARAMS]; };')
L.append('')

classes = []
for y, x, n, kind, e in objs:
    if kind != 'patchobj':
        continue
    d = e.find('object')
    ins, outs = ports[n]
    cls = 'O_' + re.sub(r'\W', '_', n)
    classes.append((n, cls))
    bufs = [f'inlet_{p}' for t, p in ins if isbuf(t)] + [f'outlet_{p}' for t, p in outs if isbuf(t)]
    sr = d.findtext('code.srate') or ''
    for b in bufs:
        sr = sr.replace(b, b + '[buffer_index]')          # Ksoloti's plain text replace
    L.append(f'struct {cls} {{')
    L.append('  rootc *parent;')
    for p in params:
        if p[1] == n:
            L.append(f'  int32_t param_{p[2]};')
    for t, p in ins:
        L.append(f'  int32_t inlet_{p}[BUFSIZE];' if isbuf(t) else f'  int32_t inlet_{p};')
    for t, p in outs:
        L.append(f'  int32_t outlet_{p}[BUFSIZE];' if isbuf(t) else f'  int32_t outlet_{p};')
    decl = SECTION.sub('', d.findtext('code.declaration') or '').replace('attr_parent', cls)
    decl = SIZEOPT.sub('', decl)
    init = SECTION.sub('', d.findtext('code.init') or '')
    # static arrays inside init (e.g. the delay memory) would be shared by every plugin instance:
    # move them into the class so each engine owns its own
    # (also inside functions in the declaration, e.g. initOs(), where the size-optimised patch keeps its init code)
    ST = r'static\s+([A-Za-z_][\w]*)\s+(\w+)\s*((?:\[[^\]]+\])+)\s*;'
    moved = []
    for m in list(re.finditer(ST, init)):
        moved.append(f'\n  {m.group(1)} {m.group(2)}{m.group(3)};   /* moved from init: one per instance */')
        init = init.replace(m.group(0), '')
    for m in list(re.finditer(ST, decl)):
        moved.append(f'\n  {m.group(1)} {m.group(2)}{m.group(3)};   /* moved from a function: one per instance */')
        decl = decl.replace(m.group(0), '')
    decl += ''.join(moved)
    kr = d.findtext('code.krate') or ''
    _p = hq(n, {'decl': decl, 'kr': kr, 'sr': sr}); decl, kr, sr = _p['decl'], _p['kr'], _p['sr']
    L.append(decl)
    L.append('  void ks_init(rootc *_parent){ parent=_parent;\n' + init + '\n  }')
    L.append('  void ks_dsp(){\n' + kr + '\n  for(int buffer_index=0;buffer_index<BUFSIZE;buffer_index++){\n' + sr + '\n  }\n  }')
    mh = d.findtext('code.midihandler') or ''
    ccs = ''.join(f'        if ((status == MIDI_CONTROL_CHANGE)&&(data1 == {p[5]})) PExParameterChange(&parent->PExch[{p[0]}],{cc_value(p[3], p[6])},0xFFFD);\n'
                  for p in params if p[1] == n and p[5] is not None)
    L.append('  void ks_midi(midi_device_t dev,uint8_t port,uint8_t status,uint8_t data1,uint8_t data2){\n' + mh + '\n' + ccs + '  }')
    L.append('};')
    L.append('')

# ---------- engine ----------
L.append('struct EngineCore {')
L.append('  rootc root;')
for n, cls in classes:
    L.append(f'  {cls} o_{re.sub(r"\\W", "_", n)};')
for cname, b, s, ds in nets:
    L.append(f'  int32_t {cname}' + ('[BUFSIZE];' if b else ';'))
L.append('  int32_t audioInL[BUFSIZE], audioOutL[BUFSIZE], audioOutR[BUFSIZE];')
L.append('  void init(){')
L.append('    this->~EngineCore(); new (this) EngineCore(); g_ks_rng = 0x12345678u;   // value-initialised: zeroed, constant tables kept')
for i, p in enumerate(params):
    L.append(f'    root.PExch[{p[0]}].value = {p[4]};')
for n, cls in classes:
    L.append(f'    o_{re.sub(r"\\W", "_", n)}.ks_init(&root);')
L.append('  }')

L.append('  // one 16-sample block: audio in (frac32, 1<<27 = full scale), stereo out')
L.append('  void process(){')
L.append('    for(int i=0;i<BUFSIZE;i++){audioOutL[i]=0;audioOutR[i]=0;}')
for y, x, n, kind, e in objs:
    ins, outs = ports[n]
    o = 'o_' + re.sub(r'\W', '_', n)
    if kind == 'patchobj':
        for p in params:
            if p[1] == n:
                L.append(f'    {o}.param_{p[2]} = root.PExch[{p[0]}].value;')
        for t, p in ins:
            ne = dst_net.get((n, p))
            if isbuf(t):
                L.append(f'    memcpy({o}.inlet_{p}, {ne[0]}, sizeof(int32buffer));' if ne else f'    memset({o}.inlet_{p}, 0, sizeof(int32buffer));')
            else:
                L.append(f'    {o}.inlet_{p} = {ne[0]};' if ne else f'    {o}.inlet_{p} = 0;')
        L.append(f'    {o}.ks_dsp();')
        for t, p in outs:
            ne = src_net.get((n, p))
            if ne:
                L.append(f'    memcpy({ne[0]}, {o}.outlet_{p}, sizeof(int32buffer));' if isbuf(t) else f'    {ne[0]} = {o}.outlet_{p};')
    else:
        t = e.get('type')
        if t == 'math/+':
            a = dst_net.get((n, 'in1')); b = dst_net.get((n, 'in2')); s = src_net.get((n, 'out'))
            if s:
                L.append(f'    {s[0]} = {a[0] if a else 0} + {b[0] if b else 0};')
        elif t == 'audio/out stereo':
            a = dst_net.get((n, 'left')); b = dst_net.get((n, 'right'))
            L.append('    for(int i=0;i<BUFSIZE;i++){' + (f'audioOutL[i]+=__SSAT({a[0]}[i],28);' if a else '') + (f'audioOutR[i]+=__SSAT({b[0]}[i],28);' if b else '') + '}')
        elif t == 'audio/in left':
            s = src_net.get((n, 'wave'))
            if s:
                L.append(f'    memcpy({s[0]}, audioInL, sizeof(int32buffer));')
L.append('  }')
L.append('  void midi(uint8_t status,uint8_t data1,uint8_t data2){')
for y, x, n, kind, e in objs:
    if kind == 'patchobj':
        L.append(f'    o_{re.sub(r"\\W", "_", n)}.ks_midi(MIDI_DEVICE_USB_HOST,1,status,data1,data2);')
L.append('  }')
L.append('};')
L.append('')
PRETTY = {'Patch_Memory':'Memory','MS20_Voices':'VCO','EG1':'EG1','EG2':'EG2','Master_Mixer':'Mixer','Filter_Macro':'Filter','VCA_Macro':'VCA',
          'PatchBay':'Patch','MS20_Master_Tune':'Master','BBD_Delay':'Delay','MG_Morphing_LFO':'MG','ESP_Layer':'ESP','MS20iC_LEDs':'LED'}
L.append('struct ParamInfo { const char *id; const char *object; const char *name; const char *label; const char *type; int32_t def; int cc; int hmax; };')
L.append('static const ParamInfo kParamInfo[NPARAMS] = {')
for p in params:
    L.append(f'  {{"{p[0]}","{p[1]}","{p[2]}","{PRETTY.get(p[1], p[1])} {p[2]}","{p[3]}",{p[4]},{p[5] if p[5] else -1},{p[6] if p[6] else 0}}},')
L.append('};')
L.append('} // namespace ms20')
open(out, 'w').write('\n'.join(L) + '\n')
print(f'{len(classes)} objects, {len(nets)} nets, {len(params)} parameters ({sum(1 for p in params if p[5])} with CC)')
