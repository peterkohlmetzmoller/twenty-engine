# v51_cables.py - 1.1, part 2: sounds remember their cables; Key Click default 6 -> 3.
# Runs after v51_edits.py:  python3 tools/v51_cables.py patch/MS20_monopoly_v51.axp patch/MS20_monopoly_v51.axp
#
# Patch_State (a new object, last in the patch, so every existing parameter keeps its number) holds the cable state
# that is not already a PatchBay parameter: VCO1+2 CV IN's source, mono, a plug in WHEEL OUT - and ovr, set while a
# recalled sound's own cables are in use. PatchBay keeps the real panel's cables apart (psel, pmono, pwplug). The
# first cable event, the panel (switch + top C), a sound without cables or switch + top C held 2 s ('cables only')
# brings the real panel's cables back.
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()

def rep(old, new, what):
    global s
    n = s.count(old)
    if n != 1: raise SystemExit(f'{what}: found {n} times')
    s = s.replace(old, new)

PS = 'parent->PExch[PARAM_INDEX_Patch__State_'

# ---- Key Click: default 6 -> 3
rep('<frac32.u.map name="clicknoise" value="6.0"/>', '<frac32.u.map name="clicknoise" value="3.0"/>', 'key click default')

# ---- the Patch_State object, placed last (y=800)
rep('\n   <nets>', '''
   <patchobj type="patch/object" uuid="b100000000000000000000000000000000000c00" name="Patch_State" x="1246" y="800">
      <params>
         <int32 name="c12" value="0"/>
         <int32 name="mono" value="0"/>
         <int32 name="wheel" value="0"/>
         <int32 name="ovr" value="0"/>
      </params>
      <attribs/>
      <object id="patch/object" uuid="b100000000000000000000000000000000000c00">
         <sDescription>Cable state that is not a PatchBay parameter (VCO1+2 CV IN source, mono, a plug in WHEEL OUT), and ovr: a recalled sound's own cables are in use. Written by PatchBay and Patch_Memory. Kept last in the patch so the other parameters keep their numbers.</sDescription>
         <inlets/>
         <outlets/>
         <displays/>
         <params>
            <int32 name="c12" description="VCO1+2 CV IN source (as the PatchBay selectors)">
               <MinValue i="0"/>
               <MaxValue i="13"/>
            </int32>
            <int32 name="mono" description="mono mode (KBD CV OUT in VCO1+2 CV IN)">
               <MinValue i="0"/>
               <MaxValue i="1"/>
            </int32>
            <int32 name="wheel" description="a plug in WHEEL OUT">
               <MinValue i="0"/>
               <MaxValue i="1"/>
            </int32>
            <int32 name="ovr" description="a recalled sound's own cables are in use (until a cable is touched)">
               <MinValue i="0"/>
               <MaxValue i="1"/>
            </int32>
         </params>
         <attribs/>
         <includes/>
         <code.declaration><![CDATA[]]></code.declaration>
         <code.krate><![CDATA[]]></code.krate>
      </object>
   </patchobj>
   <nets>''', 'Patch_State object')

# ---- PatchBay: the real panel's cables, kept apart while a sound's own cables are in use
rep('int wplug,sel12; /* a plug in WHEEL OUT; the source in VCO1+2 CV IN (not a parameter: 0 normal) */\n',
    'int wplug,sel12; /* a plug in WHEEL OUT; the source in VCO1+2 CV IN (not a parameter: 0 normal) */\n'
    'int psel[NDEST+1],pmono,pwplug,lrseq,pubc12,pubmono,pubwheel; /* the real panel\'s cables (psel[10] = VCO1+2 CV IN); ovr seen last tick; last values written to Patch_State */\n',
    'patch bay phys decl')
rep('void setd(int d,int v){if(d==10)sel12=v;else setsel(d,v);}   /* destination 10 = VCO1+2 CV IN */',
    'void setd(int d,int v){if(d<0||d>NDEST)return;if(d==NDEST&&v==OPEN)v=0;psel[d]=v;if(d==10)sel12=v;else setsel(d,v);}   /* destination 10 = VCO1+2 CV IN; psel = the real panel */\n'
    'void setps(const char*,int i,int v){PExParameterChange(&parent->PExch[i],v,0xFFFD);}\n'
    'void restorePhys(){for(int d=0;d<NDEST;d++)setsel(d,psel[d]);sel12=psel[NDEST];monof=pmono;wplug=pwplug;   /* back to the real panel\'s cables */\n'
    ' setps(0,PARAM_INDEX_Patch__State_c12,sel12);setps(0,PARAM_INDEX_Patch__State_mono,monof);setps(0,PARAM_INDEX_Patch__State_wheel,wplug);pubc12=sel12;pubmono=monof;pubwheel=wplug;}',
    'setd phys')
rep(' if(a<0||a>34||b<0||b>34)return;\n',
    ' if(a<0||a>34||b<0||b>34)return;\n'
    ' if(' + PS + 'ovr].value){restorePhys();setps(0,PARAM_INDEX_Patch__State_ovr,0);}   /* a cable touched: the recalled sound\'s cables give way to the real panel */\n',
    'event ends override')
rep('else if(t==4)wplug=1;}   /* WHEEL OUT: plugged (cleared by its plug-out, also in the power-on dump) */\n',
    'else if(t==4)wplug=1;}   /* WHEEL OUT: plugged (cleared by its plug-out, also in the power-on dump) */\n'
    ' pmono=monof;pwplug=wplug;\n',
    'phys mono wheel')
rep('for(int i=0;i<35;i++){jack[i]=0;lk[i]=0;}monof=0;pwr=1;pon=0;wplug=0;sel12=0;',
    'for(int i=0;i<35;i++){jack[i]=0;lk[i]=0;}monof=0;pwr=1;pon=0;wplug=0;sel12=0;\n'
    'for(int d=0;d<=NDEST;d++)psel[d]=0;pmono=pwplug=0;lrseq=0;pubc12=pubmono=pubwheel=-1;',
    'phys init')
rep('outlet_mono=monof;outlet_power=pwr;outlet_poweron=pon;outlet_wheelout=wplug;',
    '''// ---- a recalled sound's own cables (Patch_State ovr) or the real panel's ----
{int ov=''' + PS + '''ovr].value?1:0;
 if(inlet_rseq!=lrseq){lrseq=inlet_rseq;if(ov){restorePhys();setps(0,PARAM_INDEX_Patch__State_ovr,0);ov=0;}}   // the memory asks for the real panel's cables (panel, a sound without cables, cables only)
 int32_t c=''' + PS + '''c12].value,m=''' + PS + '''mono].value,w=''' + PS + '''wheel].value;
 if(ov){sel12=c;monof=m;wplug=w;}               // the sound's own: the real panel stays in psel/pmono/pwplug
 else{
  int sp[NDEST]={param_total,param_freq,param_vco2freq,param_hpf,param_lpf,param_vca,param_eg1trig,param_eg2trig,param_shin,param_shclock};
  for(int d=0;d<NDEST;d++)psel[d]=sp[d];        // follow the selectors (a host preset, the plugin window's cables)
  if(c!=pubc12){sel12=psel[NDEST]=c;pubc12=c;}else if(sel12!=pubc12){setps(0,PARAM_INDEX_Patch__State_c12,sel12);pubc12=sel12;}
  if(m!=pubmono){monof=pmono=m;pubmono=m;}else if(monof!=pubmono){setps(0,PARAM_INDEX_Patch__State_mono,monof);pubmono=monof;}
  if(w!=pubwheel){wplug=pwplug=w;pubwheel=w;}else if(wplug!=pubwheel){setps(0,PARAM_INDEX_Patch__State_wheel,wplug);pubwheel=wplug;}
 }}
outlet_mono=monof;outlet_power=pwr;outlet_poweron=pon;outlet_wheelout=wplug;''',
    'patch bay krate')

rep('         <inlets>\n            <frac32.bipolar name="mgtri"/>', '         <inlets>\n            <int32 name="rseq"/>\n            <frac32.bipolar name="mgtri"/>', 'patch bay rseq inlet')
rep('            <int32 name="fn"/>\n         </outlets>', '            <int32 name="fn"/>\n            <int32 name="rseq"/>\n         </outlets>', 'memory rseq outlet')
rep('outlet_fseq=fseq;outlet_fmask=fmask;outlet_fn=fn;', 'outlet_fseq=fseq;outlet_fmask=fmask;outlet_fn=fn;outlet_rseq=rseq;', 'memory rseq out')
rep('   </nets>', '      <net>\n         <source obj="Patch_Memory" outlet="rseq"/>\n         <dest obj="PatchBay" inlet="rseq"/>\n      </net>\n   </nets>', 'rseq net')

# ---- Patch_Memory: 13 cable values per sound (10 PatchBay selectors + Patch_State c12 / mono / wheel)
rep('static const int32_t MAGIC=0x4D533230; // \'MS20\'',
    'static const int32_t MAGIC=0x4D533230; // \'MS20\': knobs only (1.0)\n'
    'static const int32_t MAGIC2=0x4D533231; // \'MS21\': knobs + cables (1.1)\n'
    'static const int NC=13;            // cable values per slot: the 10 PatchBay selectors, VCO1+2 CV IN, mono, WHEEL OUT',
    'magic2')
rep('int32_t (*mem)[NP];uint8_t *valid;   // in SDRAM (internal RAM is full)',
    'int32_t (*mem)[NP];uint8_t *valid;   // in SDRAM (internal RAM is full)\n'
    'int32_t (*cab)[NC];uint8_t *hasc;    // the sound\'s cables, and whether it has any (sounds saved before 1.1 and the factory sounds: no)\n'
    'const int cidx[NC]={PARAM_INDEX_PatchBay_total,PARAM_INDEX_PatchBay_freq,PARAM_INDEX_PatchBay_vco2freq,PARAM_INDEX_PatchBay_hpf,PARAM_INDEX_PatchBay_lpf,PARAM_INDEX_PatchBay_vca,'
    'PARAM_INDEX_PatchBay_eg1trig,PARAM_INDEX_PatchBay_eg2trig,PARAM_INDEX_PatchBay_shin,PARAM_INDEX_PatchBay_shclock,PARAM_INDEX_Patch__State_c12,PARAM_INDEX_Patch__State_mono,PARAM_INDEX_Patch__State_wheel};',
    'cab decl')
rep('void flash(int mask,int count){fmask=mask;fn=count;fseq++;}',
    'void flash(int mask,int count){fmask=mask;fn=count;fseq++;}\n'
    'int rseq;   /* +1: PatchBay goes back to the real panel\'s cables */\n'
    'void setovr(int v){if(v)PExParameterChange(&' + PS + 'ovr],1,0xFFFD);else rseq++;}   /* 1: the recalled sound\'s cables; 0: ask for the real panel\'s */',
    'setovr')
rep('  for(int p=0;p<NP;p++){if(lastcc[p]>=0)setp(p,ccraw(kind[p],lastcc[p]));lock[p]=false;}\n  inmem=-1;flash(31,1);return;',
    '  for(int p=0;p<NP;p++){if(lastcc[p]>=0)setp(p,ccraw(kind[p],lastcc[p]));lock[p]=false;}\n  setovr(0);inmem=-1;flash(31,1);return;',
    'panel cables')
rep(' for(int p=0;p<NP;p++){setp(p,mem[n][p]);if(kind[p]!=2){lock[p]=true;cur[p]=mem[n][p];}}\n inmem=n;flash(31,1);',
    ' for(int p=0;p<NP;p++){setp(p,mem[n][p]);if(kind[p]!=2){lock[p]=true;cur[p]=mem[n][p];}}\n'
    ' if(hasc[n]){setovr(1);for(int c=0;c<NC;c++)PExParameterChange(&parent->PExch[cidx[c]],cab[n][c],0xFFFD);}   /* the sound\'s own cables, until a cable is touched */\n'
    ' else setovr(0);                                                       /* no cables stored: the cables plugged in now */\n'
    ' inmem=n;flash(31,1);',
    'recall cables')
rep(' if(n>=NS)return;\n for(int p=0;p<NP;p++)mem[n][p]=parent->PExch[pidx[p]].value;\n valid[n]=1;inmem=n;savereq=1;',
    ' if(n>=NS){setovr(0);flash(31,2);return;}   /* top C held 2 s: cables only - the cables plugged in now, knobs as they are */\n'
    ' for(int p=0;p<NP;p++)mem[n][p]=parent->PExch[pidx[p]].value;\n'
    ' for(int c=0;c<NC;c++)cab[n][c]=parent->PExch[cidx[c]].value;hasc[n]=1;   /* the cables as heard */\n'
    ' valid[n]=1;inmem=n;savereq=1;',
    'save cables')
rep(' if(ty==6){if(a!=b){for(int p=0;p<NP;p++)lock[p]=false;inmem=-1;}return;}',
    ' if(ty==6){if(a!=b){for(int p=0;p<NP;p++)lock[p]=false;inmem=-1;setovr(0);}return;}',
    'power on cables')
# file: 'MS21' + NS + NP + NC, then per slot: valid, NP knobs, hasc, NC cables. 'MS20' files (1.0) still load.
rep(''' int32_t hdr[3];
 if(f_read(&f,fbuff,12,&br)!=FR_OK||br!=12){f_close(&f);return false;}
 memcpy(hdr,fbuff,12);
 if(hdr[0]!=MAGIC||hdr[1]!=NS||hdr[2]!=NP){f_close(&f);return false;}
 for(int n=0;n<NS;n++){
  if(f_read(&f,fbuff,(NP+1)*4,&br)!=FR_OK||br!=(UINT)((NP+1)*4))break;
  if(fbuff[0]==1){memcpy(mem[n],&fbuff[1],NP*4);valid[n]=1;}
 }''',
''' int32_t hdr[4];
 if(f_read(&f,fbuff,12,&br)!=FR_OK||br!=12){f_close(&f);return false;}
 memcpy(hdr,fbuff,12);
 if((hdr[0]!=MAGIC&&hdr[0]!=MAGIC2)||hdr[1]!=NS||hdr[2]!=NP){f_close(&f);return false;}
 int nc=0;
 if(hdr[0]==MAGIC2){if(f_read(&f,fbuff,4,&br)!=FR_OK||br!=4){f_close(&f);return false;}memcpy(&hdr[3],fbuff,4);nc=hdr[3];if(nc!=NC){f_close(&f);return false;}}
 UINT rl=(UINT)((NP+1+(nc?nc+1:0))*4);           // one slot: valid, knobs (+ has cables, cables)
 for(int n=0;n<NS;n++){
  if(f_read(&f,fbuff,rl,&br)!=FR_OK||br!=rl)break;
  if(fbuff[0]==1){memcpy(mem[n],&fbuff[1],NP*4);valid[n]=1;}
  if(nc&&fbuff[0]==1&&fbuff[NP+1]==1){memcpy(cab[n],&fbuff[NP+2],NC*4);hasc[n]=1;}
 }''', 'load file')
rep(''' int32_t hdr[3]={MAGIC,NS,NP};memcpy(fbuff,hdr,12);f_write(&f,fbuff,12,&bw);tot+=bw;
 for(int n=0;n<NS;n++){
  fbuff[0]=valid[n];memcpy(&fbuff[1],mem[n],NP*4);f_write(&f,fbuff,(NP+1)*4,&bw);tot+=bw;
 }
 return (f_close(&f)==FR_OK)&&(tot==(UINT)(12+NS*(NP+1)*4));''',
''' int32_t hdr[4]={MAGIC2,NS,NP,NC};memcpy(fbuff,hdr,16);f_write(&f,fbuff,16,&bw);tot+=bw;
 for(int n=0;n<NS;n++){
  fbuff[0]=valid[n];memcpy(&fbuff[1],mem[n],NP*4);fbuff[NP+1]=hasc[n];memcpy(&fbuff[NP+2],cab[n],NC*4);
  f_write(&f,fbuff,(NP+2+NC)*4,&bw);tot+=bw;
 }
 return (f_close(&f)==FR_OK)&&(tot==(UINT)(16+NS*(NP+2+NC)*4));''', 'save file')
rep('mem=_mem;valid=_valid;',
    'mem=_mem;valid=_valid;\n'
    'static int32_t _cab[NS][NC] __attribute__((section(".sdram")));static uint8_t _hasc[NS] __attribute__((section(".sdram")));\n'
    'cab=_cab;hasc=_hasc;rseq=0;for(int n=0;n<NS;n++){hasc[n]=0;for(int c=0;c<NC;c++)cab[n][c]=0;}',
    'cab init')

open(dst, 'w', encoding='utf-8').write(s)
print('ok')
