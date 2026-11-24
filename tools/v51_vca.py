# v51_vca.py - 1.1, part 4: the MS-20's modulation VCA (VCA IN, CONTROL IN, VCA OUT).
# Runs after v51_drone.py:  python3 tools/v51_vca.py patch/MS20_monopoly_v51.axp patch/MS20_monopoly_v51.axp
# VCA OUT = the source in VCA IN x the source in CONTROL IN (0..1). Nothing in CONTROL IN: closed, as on the MS-20ic.
# CONTROL IN keeps controlling the main VCA (tremolo, as before 1.1) until VCA OUT is patched somewhere; then it
# controls the modulation VCA only. VCA OUT is source 5 (was EG2's own output, which has no jack on the MS-20).
# The VCA IN source is kept with the other cables (Patch_State vin, stored with sounds: 15 cable values).
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()

def rep(old, new, what):
    global s
    n = s.count(old)
    if n != 1: raise SystemExit(f'{what}: found {n} times')
    s = s.replace(old, new)

# ---- patch bay: destination 12 = VCA IN; source 5 = VCA OUT
rep('int psel[NDEST+2],pmono,pwplug,lrseq,pubc12,pubmono,pubwheel,selig,pubig;',
    'int psel[NDEST+3],pmono,pwplug,lrseq,pubc12,pubmono,pubwheel,selig,pubig,selvin,pubvin;   /* selvin: the source in VCA IN (12) */', 'decl')
rep('void setd(int d,int v){if(d<0||d>NDEST+1)return;if(d>=NDEST&&v==OPEN)v=0;psel[d]=v;if(d==10)sel12=v;else if(d==11)selig=v;else setsel(d,v);}',
    'void setd(int d,int v){if(d<0||d>NDEST+2)return;if(d>=NDEST&&v==OPEN)v=0;psel[d]=v;if(d==10)sel12=v;else if(d==11)selig=v;else if(d==12)selvin=v;else setsel(d,v);}   /* 12 = VCA IN */', 'setd')
rep('sel12=psel[NDEST];selig=psel[NDEST+1];monof=pmono;', 'sel12=psel[NDEST];selig=psel[NDEST+1];selvin=psel[NDEST+2];monof=pmono;', 'restore')
rep('pubwheel=wplug;setps(0,PARAM_INDEX_Patch__State_ig,selig);pubig=selig;}', 'pubwheel=wplug;setps(0,PARAM_INDEX_Patch__State_ig,selig);pubig=selig;setps(0,PARAM_INDEX_Patch__State_vin,selvin);pubvin=selvin;}', 'restore publish')
rep('for(int d=0;d<=NDEST+1;d++)psel[d]=0;pmono=pwplug=0;lrseq=0;selig=0;pubc12=pubmono=pubwheel=pubig=-1;',
    'for(int d=0;d<=NDEST+2;d++)psel[d]=0;pmono=pwplug=0;lrseq=0;selig=0;selvin=0;pubc12=pubmono=pubwheel=pubig=pubvin=-1;', 'init')
rep('jack[21]=-12; // INITIAL GAIN in (drone)', 'jack[21]=-12; // INITIAL GAIN in (drone)\njack[12]=-13; // VCA IN (modulation VCA)\njack[16]=5;   // VCA OUT (modulation VCA)', 'jacks')
rep('// not routed: 3 ESP signal in, 8 amp out, 9 ext signal in, 12 VCA in, 15 BPF out, 16 VCA out, 20 F-V out,',
    '// not routed: 3 ESP signal in, 8 amp out, 9 ext signal in, 15 BPF out, 20 F-V out,', 'jack comment')
rep(',g=parent->PExch[PARAM_INDEX_Patch__State_ig].value;', ',g=parent->PExch[PARAM_INDEX_Patch__State_ig].value,vi=parent->PExch[PARAM_INDEX_Patch__State_vin].value;', 'read vin')
rep('if(ov){sel12=c;monof=m;wplug=w;selig=g;}', 'if(ov){sel12=c;monof=m;wplug=w;selig=g;selvin=vi;}', 'ovr vin')
rep('  if(g!=pubig){selig=psel[NDEST+1]=g;pubig=g;}else if(selig!=pubig){setps(0,PARAM_INDEX_Patch__State_ig,selig);pubig=selig;}\n',
    '  if(g!=pubig){selig=psel[NDEST+1]=g;pubig=g;}else if(selig!=pubig){setps(0,PARAM_INDEX_Patch__State_ig,selig);pubig=selig;}\n'
    '  if(vi!=pubvin){selvin=psel[NDEST+2]=vi;pubvin=vi;}else if(selvin!=pubvin){setps(0,PARAM_INDEX_Patch__State_vin,selvin);pubvin=selvin;}\n', 'mirror vin')
rep('src[5]=inlet_eg2;src[6]=(1<<27)-inlet_eg2;', 'src[6]=(1<<27)-inlet_eg2;   /* src[5] = VCA OUT, below */', 'src5')
rep('int clk=(shc>0);if(clk&&!pclk)shval=shi;pclk=clk;src[7]=shval;\n',
    'int clk=(shc>0);if(clk&&!pclk)shval=shi;pclk=clk;src[7]=shval;\n'
    '// ---- modulation VCA: OUT = VCA IN x CONTROL IN (0..1); nothing in CONTROL IN: closed ----\n'
    '{int vc=param_vca;int32_t vgain=(vc>0&&vc<OPEN)?src[vc]:0;if(vgain<0)vgain=0;if(vgain>(1<<27))vgain=1<<27;\n'
    ' int32_t vx=(selvin>0&&selvin<OPEN&&selvin!=5)?src[selvin]:0;src[5]=(int32_t)(((int64_t)vx*vgain)>>27);}\n', 'vca')
rep('outlet_total=r[0];outlet_freq=r[1];outlet_vco2freq=r[2];outlet_hpf=r[3];outlet_lpf=r[4];outlet_vca=r[5];outlet_eg1trig=r[6];outlet_eg2trig=r[7];',
    '{int vu=(sel12==5||selig==5||param_shin==5||param_shclock==5);for(int d=0;d<8;d++)if(d!=5&&sel[d]==5)vu=1;   /* VCA OUT patched: CONTROL IN is the modulation VCA\'s */\n'
    ' if(vu)r[5]=NORMAL;}\n'
    'outlet_total=r[0];outlet_freq=r[1];outlet_vco2freq=r[2];outlet_hpf=r[3];outlet_lpf=r[4];outlet_vca=r[5];outlet_eg1trig=r[6];outlet_eg2trig=r[7];', 'main vca')

# ---- Patch_State: vin (index 63, after ig)
rep('         <int32 name="ig" value="0"/>\n      </params>', '         <int32 name="ig" value="0"/>\n         <int32 name="vin" value="0"/>\n      </params>', 'vin value')
rep('''            <int32 name="ig" description="INITIAL GAIN source (as the PatchBay selectors): drone">
               <MinValue i="0"/>
               <MaxValue i="13"/>
            </int32>''', '''            <int32 name="ig" description="INITIAL GAIN source (as the PatchBay selectors): drone">
               <MinValue i="0"/>
               <MaxValue i="13"/>
            </int32>
            <int32 name="vin" description="VCA IN source (as the PatchBay selectors): the modulation VCA">
               <MinValue i="0"/>
               <MaxValue i="13"/>
            </int32>''', 'vin param')

# ---- memory: 15 cable values per sound; files with fewer (1.1 test builds) still load
rep('static const int NC=14;            // cable values per slot: the 10 PatchBay selectors, VCO1+2 CV IN, mono, WHEEL OUT, INITIAL GAIN',
    'static const int NC=15;            // cable values per slot: the 10 PatchBay selectors, VCO1+2 CV IN, mono, WHEEL OUT, INITIAL GAIN, VCA IN', 'NC 15')
rep('PARAM_INDEX_Patch__State_wheel,PARAM_INDEX_Patch__State_ig};', 'PARAM_INDEX_Patch__State_wheel,PARAM_INDEX_Patch__State_ig,PARAM_INDEX_Patch__State_vin};', 'cidx vin')
rep('nc=hdr[3];if(nc!=NC){f_close(&f);return false;}}', 'nc=hdr[3];if(nc<1||nc>NC){f_close(&f);return false;}}   /* fewer cables (older file): the rest stay normal */', 'nc check')
rep('if(nc&&fbuff[0]==1&&fbuff[NP+1]==1){memcpy(cab[n],&fbuff[NP+2],NC*4);hasc[n]=1;}',
    'if(nc&&fbuff[0]==1&&fbuff[NP+1]==1){memset(cab[n],0,NC*4);memcpy(cab[n],&fbuff[NP+2],nc*4);hasc[n]=1;}', 'nc copy')

# the patch bay comment
rep('5 EG2 6 EG2rev', '5 VCA out 6 EG2rev', 'comment 1')
rep('4 EG1 rev, 5 EG2, 6 EG2 rev', '4 EG1 rev, 5 VCA out, 6 EG2 rev', 'comment 2')

open(dst, 'w', encoding='utf-8').write(s)
print('ok')
