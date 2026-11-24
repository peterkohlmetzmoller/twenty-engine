# v51_drone.py - 1.1, part 3: INITIAL GAIN = a drone voice.
# Runs after v51_cables.py:  python3 tools/v51_drone.py patch/MS20_monopoly_v51.axp patch/MS20_monopoly_v51.axp
# A cable in INITIAL GAIN holds the voice of the last note played open at the source's level (0..1; negative = 0),
# like the MS-20's VCA initial gain. Before any key it drones on that voice's start note (E). Its source is kept with
# the other cables (Patch_State ig, stored with sounds).
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()

def rep(old, new, what):
    global s
    n = s.count(old)
    if n != 1: raise SystemExit(f'{what}: found {n} times')
    s = s.replace(old, new)

PS = 'parent->PExch[PARAM_INDEX_Patch__State_'

# ---- voices: the drone
rep('volatile bool mono_on;', 'volatile bool mono_on;int dv; /* drone voice: the last note played */', 'dv decl')
rep('mono_on=false;mg_was=0;', 'mono_on=false;dv=0;mg_was=0;', 'dv init')
rep('vp[tv]=np;tg[tv]=np;', 'vp[tv]=np;tg[tv]=np;dv=tv;', 'dv on note')
rep('int act=0,ag=0;\n',
    'float igl=0.f;if(inlet_igain!=(int32_t)0x80000000){igl=inlet_igain*7.4505806e-9f;if(igl<0.f)igl=0.f;if(igl>1.f)igl=1.f;}   /* INITIAL GAIN: drone level */\n'
    'int dvx=mono_on?0:dv;int act=0,ag=0;\n', 'drone level')
rep('  ve[v]=vg[v]?1.f:0.f; // organ gate: on/off, the srate smoother only removes the click\n }',
    '  ve[v]=vg[v]?1.f:0.f; // organ gate: on/off, the srate smoother only removes the click\n }\n'
    ' if(igl>0.f&&v==dvx){if(vs[v]==0)vs[v]=3;if(ve[v]<igl)ve[v]=igl;}   /* INITIAL GAIN patched: the last note\'s voice never closes below the drone level */',
    'drone floor')
rep('            <frac32.bipolar name="vco12cv"/>\n         </inlets>',
    '            <frac32.bipolar name="vco12cv"/>\n            <frac32.bipolar name="igain"/>\n         </inlets>', 'voices igain inlet')

# ---- patch bay: destination 11 = INITIAL GAIN (not a parameter, like VCO1+2 CV IN)
rep('            <frac32.bipolar name="vco12cv"/>\n         </outlets>',
    '            <frac32.bipolar name="vco12cv"/>\n            <frac32.bipolar name="igain"/>\n         </outlets>', 'patch bay igain outlet')
rep('int psel[NDEST+1],pmono,pwplug,lrseq,pubc12,pubmono,pubwheel;', 'int psel[NDEST+2],pmono,pwplug,lrseq,pubc12,pubmono,pubwheel,selig,pubig;', 'psel size')
rep('void setd(int d,int v){if(d<0||d>NDEST)return;if(d==NDEST&&v==OPEN)v=0;psel[d]=v;if(d==10)sel12=v;else setsel(d,v);}',
    'void setd(int d,int v){if(d<0||d>NDEST+1)return;if(d>=NDEST&&v==OPEN)v=0;psel[d]=v;if(d==10)sel12=v;else if(d==11)selig=v;else setsel(d,v);}   /* 11 = INITIAL GAIN */',
    'setd ig')
rep('sel12=psel[NDEST];monof=pmono;wplug=pwplug;', 'sel12=psel[NDEST];selig=psel[NDEST+1];monof=pmono;wplug=pwplug;', 'restore ig')
rep('pubc12=sel12;pubmono=monof;pubwheel=wplug;}', 'pubc12=sel12;pubmono=monof;pubwheel=wplug;setps(0,PARAM_INDEX_Patch__State_ig,selig);pubig=selig;}', 'restore publish ig')
rep('for(int d=0;d<=NDEST;d++)psel[d]=0;pmono=pwplug=0;lrseq=0;pubc12=pubmono=pubwheel=-1;',
    'for(int d=0;d<=NDEST+1;d++)psel[d]=0;pmono=pwplug=0;lrseq=0;selig=0;pubc12=pubmono=pubwheel=pubig=-1;', 'init ig')
rep('wheel].value;\n if(ov){sel12=c;monof=m;wplug=w;}',
    'wheel].value,g=' + PS + 'ig].value;\n if(ov){sel12=c;monof=m;wplug=w;selig=g;}', 'krate ig ov')
rep('setps(0,PARAM_INDEX_Patch__State_wheel,wplug);pubwheel=wplug;}\n }}',
    'setps(0,PARAM_INDEX_Patch__State_wheel,wplug);pubwheel=wplug;}\n'
    '  if(g!=pubig){selig=psel[NDEST+1]=g;pubig=g;}else if(selig!=pubig){setps(0,PARAM_INDEX_Patch__State_ig,selig);pubig=selig;}\n }}',
    'krate ig publish')
rep('/* KBD CV in VCO1+2 CV IN is mono mode, not pitch */',
    '/* KBD CV in VCO1+2 CV IN is mono mode, not pitch */\noutlet_igain=(selig>0&&selig<OPEN)?src[selig]:NORMAL;   /* INITIAL GAIN: drone level */',
    'igain out')
rep('jack[25]=-11; // VCO1+2 CV in (KBD CV here = mono mode)', 'jack[25]=-11; // VCO1+2 CV in (KBD CV here = mono mode)\njack[21]=-12; // INITIAL GAIN in (drone)', 'jack 21')
rep('//             21 initial gain, 28 ESP env out,', '//             28 ESP env out,', 'jack comment')

# ---- Patch_State: ig (index 62, after ovr)
rep('         <int32 name="ovr" value="0"/>\n      </params>', '         <int32 name="ovr" value="0"/>\n         <int32 name="ig" value="0"/>\n      </params>', 'ig param value')
rep('''            <int32 name="ovr" description="a recalled sound's own cables are in use (until a cable is touched)">
               <MinValue i="0"/>
               <MaxValue i="1"/>
            </int32>''', '''            <int32 name="ovr" description="a recalled sound's own cables are in use (until a cable is touched)">
               <MinValue i="0"/>
               <MaxValue i="1"/>
            </int32>
            <int32 name="ig" description="INITIAL GAIN source (as the PatchBay selectors): drone">
               <MinValue i="0"/>
               <MaxValue i="13"/>
            </int32>''', 'ig param')

# ---- memory: 14 cable values per sound
rep('static const int NC=13;            // cable values per slot: the 10 PatchBay selectors, VCO1+2 CV IN, mono, WHEEL OUT',
    'static const int NC=14;            // cable values per slot: the 10 PatchBay selectors, VCO1+2 CV IN, mono, WHEEL OUT, INITIAL GAIN', 'NC 14')
rep('PARAM_INDEX_Patch__State_mono,PARAM_INDEX_Patch__State_wheel};', 'PARAM_INDEX_Patch__State_mono,PARAM_INDEX_Patch__State_wheel,PARAM_INDEX_Patch__State_ig};', 'cidx ig')

rep('   </nets>', '      <net>\n         <source obj="PatchBay" outlet="igain"/>\n         <dest obj="MS20_Voices" inlet="igain"/>\n      </net>\n   </nets>', 'igain net')

open(dst, 'w', encoding='utf-8').write(s)
print('ok')
