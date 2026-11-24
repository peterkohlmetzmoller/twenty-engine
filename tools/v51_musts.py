# v51_musts.py - 1.1, part 5: retrigger in mono, ESP knob pickup and latch, the function switch, TRIG LED = delay.
# Runs after v51_vca.py:  python3 tools/v51_musts.py patch/MS20_monopoly_v51.axp patch/MS20_monopoly_v51.axp
#  - Retrigger (VCO multitrig, the window's RETRIG switch / a Teensy button): in mono every new key starts the
#    envelopes again (the keyboard gate drops for one control tick, so EG 1 and EG 2 attack again), like the
#    MS-20's multiple trigger. In poly it already restarted EG 2 on every key.
#  - ESP knobs: after the knobs change layer (switch held / released, arp on / off, latch) each knob takes over
#    once it reaches the value it now sets (pickup), so the delay time no longer jumps to where the arp rate was.
#    The panel read at power-on catches them all.
#  - CC 105: the ESP layer latch (127 = delay layer as if the switch were held). The window's DELAY latch and the
#    Teensy ESP LAYER button send it, so the iC's ESP knobs follow.
#  - CC 104: the function switch (a Teensy button): from the first one on, memory (switch + key) and the ESP
#    layers / tap tempo listen to it; the iC's own switch is then only the patch bay SWITCH (and still taps with the
#    SWITCH -> ESP SIGNAL IN cable).
#  - CC 106: one tap of tap tempo (a Teensy TAP button), with or without the SWITCH -> ESP SIGNAL IN cable.
#  - ESP TRIG LED: blinks at the delay time while DELAY AMOUNT is up (was: slow blink on a stored sound); held
#    switch and saving still light / flash it.
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()

def rep(old, new, what, n=1):
    global s
    c = s.count(old)
    if c != n: raise SystemExit(f'{what}: found {c} times')
    s = s.replace(old, new)

# ---- MS20_Voices: retrigger in mono
rep('int mg_was,mhp;float mtg,mpos,p1a;', 'int mg_was,mhp,mnk,mrt;float mtg,mpos,p1a;', 'mono decl')
rep('mono_on=false;dv=0;mg_was=0;mhp=0;', 'mono_on=false;dv=0;mg_was=0;mhp=0;mnk=0;mrt=0;', 'mono init')
rep(' lon=kclk;\n if(!mono_on){', ' lon=kclk;if(mono_on)mnk=1;   /* mono: a new key (retrigger) */\n if(!mono_on){', 'mono new key')
rep('   if(!mg_was){vs[0]=1;vh[0]=holdt;va[0]=++acnt;if(!mhp){mpos=mtg;mhp=1;}} /* first key: attack; legato keys only change pitch */',
    '   {int nk=mnk;mnk=0;if(!mg_was||(param_multitrig&&nk)){if(mg_was)mrt=1;vs[0]=1;vh[0]=holdt;va[0]=++acnt;if(!mhp){mpos=mtg;mhp=1;}}} /* first key: attack; legato keys only change pitch - with retrigger every new key attacks again */',
    'mono retrigger')
rep('outlet_polygate=ag;outlet_activevoices=act;', 'outlet_polygate=mrt?0:ag;mrt=0;outlet_activevoices=act;   /* retrigger in mono: the gate drops for one tick, both EGs attack again */', 'gate drop')

# ---- ESP_Layer: pickup, latch, function switch, power-on catch
rep('''         <inlets>
            <int32 name="mode"/>
            <bool32 name="tapok"/>
         </inlets>
         <outlets>
            <bool32 name="layerb"/>''', '''         <inlets>
            <int32 name="mode"/>
            <bool32 name="tapok"/>
            <int32 name="poweron"/>
         </inlets>
         <outlets>
            <bool32 name="layerb"/>''', 'esp inlets')
rep('int pidx[NSET][NK];int zoned[NSET][NK]; // zoned: 4-step selector (arp octaves / order), else a 0..64 knob',
    'int pidx[NSET][NK];int zoned[NSET][NK]; // zoned: 4-step selector (arp octaves / order), else a 0..64 knob\n'
    'bool caught[NSET][NK];int lastl,lcc[NK];int32_t lpon;bool lat,fsw; /* pickup per layer; lcc: each knob\'s last position; lat: layer latch (CC 105); fsw: a function switch (CC 104) is in use */',
    'esp decl')
rep('int curset(){bool held=(nhard>0||btn);', 'int curset(){bool held=(nhard>0||btn||lat);', 'esp latch')
rep('''void knob(int k,int cc){
 int l=curset();
 if(zoned[l][k])PExParameterChange(&parent->PExch[pidx[l][k]],cc>>5,0xFFFD);                                   // 4 zones
 else PExParameterChange(&parent->PExch[pidx[l][k]],(cc!=127)?(cc<<20):0x07FFFFFF,0xFFFD);                       // same scaling as Ksoloti's built-in CC assignment
}''', '''void knob(int k,int cc){
 int l=curset();
 if(l!=lastl){if(lastl>=0)for(int j=0;j<NK;j++)caught[l][j]=false;lastl=l;}   /* new layer: the knobs set other things now - each takes over when it reaches its value (pickup) */
 if(!caught[l][k]){
  int32_t v=parent->PExch[pidx[l][k]].value;int pc=zoned[l][k]?(int)v:(int)(v>>20),c=zoned[l][k]?(cc>>5):cc;
  int lc=(lcc[k]<0)?-1:(zoned[l][k]?(lcc[k]>>5):lcc[k]);
  bool met=zoned[l][k]?(c==pc):(c-pc<=2&&pc-c<=2);
  if(lc>=0&&((lc<=pc&&c>=pc)||(lc>=pc&&c<=pc)))met=true;                     // passed the value
  lcc[k]=cc;if(!met)return;
  caught[l][k]=true;
 }
 lcc[k]=cc;
 if(zoned[l][k])PExParameterChange(&parent->PExch[pidx[l][k]],cc>>5,0xFFFD);                                   // 4 zones
 else PExParameterChange(&parent->PExch[pidx[l][k]],(cc!=127)?(cc<<20):0x07FFFFFF,0xFFFD);                       // same scaling as Ksoloti's built-in CC assignment
}''', 'esp pickup')
rep('''uint8_t st=status&0xF0;
if(st==MIDI_NOTE_ON&&data2>0){
 if(data2>124){''', '''uint8_t st=status&0xF0;
if(st==MIDI_CONTROL_CHANGE&&data1==104){fsw=true;st=0xD0;data1=data2;}   /* the function switch (CC 104): as the momentary switch for the layers and taps */
else if(st==0xD0&&fsw&&!tapm)return;                                       /* with a function switch the iC's switch is the patch bay SWITCH only (tap cable: still taps) */
if(st==MIDI_CONTROL_CHANGE&&data1==105){lat=(data2>0);return;}             /* ESP layer latch: the delay layer as if the switch were held */
if(st==MIDI_CONTROL_CHANGE&&data1==106){if(data2>0)tap(etick);return;}      /* a tap tempo button (Teensy TAP): a tap, with or without the tap cable */
if(st==MIDI_NOTE_ON&&data2>0){
 if(data2>124){''', 'esp midi')
rep('nhard=0;btn=false;amode=0;tapm=0;etick=0;tpress=tlast=0;knobbed=false;ntap=0;for(int i=0;i<3;i++)tiv[i]=0.f;',
    'nhard=0;btn=false;amode=0;tapm=0;etick=0;tpress=tlast=0;knobbed=false;ntap=0;for(int i=0;i<3;i++)tiv[i]=0.f;\n'
    'lastl=-1;lpon=0;lat=false;fsw=false;for(int k=0;k<NK;k++)lcc[k]=-1;for(int l=0;l<NSET;l++)for(int k=0;k<NK;k++)caught[l][k]=true;',
    'esp init')
rep('''amode=inlet_mode;                 /* the MIDI handler can't read inlets: keep the arp mode here */''',
    '''amode=inlet_mode;                 /* the MIDI handler can't read inlets: keep the arp mode here */
if(inlet_poweron!=lpon){lpon=inlet_poweron;for(int l=0;l<NSET;l++)for(int k=0;k<NK;k++)caught[l][k]=true;}   /* the panel read at power-on: every knob is where it is */''',
    'esp poweron')

# ---- Patch_Memory: the function switch
rep('bool btn,armed,saved,tapmode;uint32_t tk,tpress,kdown;int keyn,inmem;', 'bool btn,armed,saved,tapmode,fsw;uint32_t tk,tpress,kdown;int keyn,inmem;', 'mem decl')
rep('btn=armed=saved=tapmode=false;tk=tpress=kdown=0;', 'btn=armed=saved=tapmode=fsw=false;tk=tpress=kdown=0;', 'mem init')
rep('''uint8_t st=status&0xF0;
if(st==MIDI_CONTROL_CHANGE&&(data1==102||data1==103)){''', '''uint8_t st=status&0xF0;
if(st==MIDI_CONTROL_CHANGE&&data1==104){fsw=true;st=0xD0;data1=data2;}   /* the function switch (CC 104): switch + key from now on */
else if(st==0xD0&&fsw)return;                                              /* the iC's own switch: patch bay SWITCH only */
if(st==MIDI_CONTROL_CHANGE&&(data1==102||data1==103)){''', 'mem midi')
rep('//   top C = panel (the sound jumps to the knob positions). TRIG LED blinks slowly while a stored sound is active.',
    '//   top C = panel (the sound jumps to the knob positions). (1.1: the TRIG LED shows the delay time otherwise.)', 'mem comment')

# ---- BBD_Delay -> LEDs: the delay time for the TRIG LED
rep('''         <outlets>
            <frac32buffer name="outL"/>
            <frac32buffer name="outR"/>
         </outlets>''', '''         <outlets>
            <frac32buffer name="outL"/>
            <frac32buffer name="outR"/>
            <int32 name="dtick"/>
            <int32 name="dwet"/>
         </outlets>''', 'bbd outlets')
rep('float mx=kn_drywet*7.45e-9f; float mc=mx*mx; float dg=1.0f-mc*mx; float bw=mc*0.85f;',
    'float mx=kn_drywet*7.45e-9f; float mc=mx*mx; float dg=1.0f-mc*mx; float bw=mc*0.85f;\n'
    'outlet_dtick=(int32_t)(tt*0.0625f);outlet_dwet=(mx>0.03f)?1:0;   /* the TRIG LED: the delay time in control ticks, and whether the repeats are heard */',
    'bbd time out')
rep('''            <int32 name="fn"/>
         </inlets>
         <outlets/>''', '''            <int32 name="fn"/>
            <int32 name="dtick"/>
            <int32 name="dwet"/>
         </inlets>
         <outlets/>''', 'led inlets')
rep('uint32_t tick,lastt,tint,aph;', 'uint32_t tick,lastt,tint,aph,dph;', 'led decl')
rep('tick=lastt=tint=aph=0;', 'tick=lastt=tint=aph=dph=0;', 'led init')
rep('''{int tm=inlet_trigmode;                           // ESP TRIG LED = patch memory: 1 switch held, 2 key held for saving (fast blink), 3 on a stored sound (slow blink)
 if(tm==1)want[3]=1;
 else if(tm==2)want[3]=((tick/180)&1)?0:1;        // 60 ms on / 60 ms off
 else if(tm==3)want[3]=((tick%3000)<1500)?1:0;    // 0.5 s on / 0.5 s off
 else want[3]=0;}''', '''{int tm=inlet_trigmode;                           // ESP TRIG LED: 1 switch held, 2 key held for saving (fast blink); else the delay time
 if(tm==1)want[3]=1;
 else if(tm==2)want[3]=((tick/180)&1)?0:1;        // 60 ms on / 60 ms off
 else if(inlet_dwet&&inlet_dtick>0){uint32_t dt=(uint32_t)inlet_dtick;if(++dph>=dt)dph=0;uint32_t lit=dt>>1;if(lit>150)lit=150;if(lit<15)lit=15;want[3]=(dph<lit)?1:0;}   /* a flash on every repeat while DELAY AMOUNT is up */
 else{dph=0;want[3]=0;}}''', 'led trig')

# ---- nets
rep('''         <source obj="PatchBay" outlet="poweron"/>
         <dest obj="MS20iC_LEDs" inlet="poweron"/>''', '''         <source obj="PatchBay" outlet="poweron"/>
         <dest obj="MS20iC_LEDs" inlet="poweron"/>
         <dest obj="ESP_Layer" inlet="poweron"/>''', 'poweron net')
rep('   </nets>', '''      <net>
         <source obj="BBD_Delay" outlet="dtick"/>
         <dest obj="MS20iC_LEDs" inlet="dtick"/>
      </net>
      <net>
         <source obj="BBD_Delay" outlet="dwet"/>
         <dest obj="MS20iC_LEDs" inlet="dwet"/>
      </net>
   </nets>''', 'delay nets')

open(dst, 'w', encoding='utf-8').write(s)
print('ok')
