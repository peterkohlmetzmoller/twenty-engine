# v51_edits.py - 1.1 engine changes, applied to the Ksoloti patch (v50c -> v51). Every edit must match exactly once.
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()

def rep(old, new, what):
    global s
    n = s.count(old)
    if n != 1: raise SystemExit(f'{what}: found {n} times')
    s = s.replace(old, new)

# 1. MG TRI OUT follows the WAVEFORM knob (the shaped MG, before TOTAL), as on the original MS-20
rep('outlet_tri=(int32_t)(sw*134217728.f);outlet_sq=(int32_t)(sq*134217728.f); // MG outs for the patch bay',
    'outlet_tri=(int32_t)(lfo*134217728.f);outlet_sq=(int32_t)(sq*134217728.f); // MG outs for the patch bay: TRI follows WAVEFORM, SQ stays square (S&H clock)',
    'MG TRI OUT')

# 2. mono + arp: VCO 1's phaser saw (the arp's VCO 1 gets its own phaser, same settings as the voices')
rep('float ap_s[12],ph_fb,ph_a,ph_da;bool phwas;',
    'float ap_s[12],ph_fb,ph_a,ph_da;bool phwas;float ap2_s[12],ph_fb2; /* ap2: phaser on the mono-arp VCO 1 */',
    'phaser decl')
rep('for(int k=0;k<12;k++)ap_s[k]=0.f;ph_fb=ph_a=ph_da=0.f;phwas=false;',
    'for(int k=0;k<12;k++)ap_s[k]=0.f;ph_fb=ph_a=ph_da=0.f;phwas=false;for(int k=0;k<12;k++)ap2_s[k]=0.f;ph_fb2=0.f;',
    'phaser init')
rep(' if(!phwas){for(int k=0;k<12;k++)ap_s[k]=0.f;ph_fb=0.f;ph_a=tgt;}',
    ' if(!phwas){for(int k=0;k<12;k++){ap_s[k]=0.f;ap2_s[k]=0.f;}ph_fb=ph_fb2=0.f;ph_a=tgt;}',
    'phaser reset')
rep(' else ao1=ns;\n}\noutlet_monov1=',
    ' else ao1=ns;\n if(phon){float a=ph_a;float y=ao1+phres*softc(ph_fb2);   /* the same phaser as the voices (w1==1 only) */\n  for(int k=0;k<12;k++){float o=a*y+ap2_s[k];ap2_s[k]=y-a*o;y=o;}\n  ph_fb2=y;ao1=ao1*(1.f-phmix)+y*phmix;}\n}\noutlet_monov1=',
    'mono-arp phaser')

# 3. wheel: faster response (~16 ms instead of ~80 ms), and no vibrato while a cable is in WHEEL OUT
rep('mws+=0.004f*(mw-mws);', 'mws+=0.02f*(mw-mws);', 'wheel smoothing')
rep('int32_t vib=(int32_t)(bh(vph)*(mws*mws*0.5f)*2097152.f);',
    'int32_t vib=inlet_wheelout?0:(int32_t)(bh(vph)*(mws*mws*0.5f)*2097152.f); /* WHEEL OUT patched: the wheel is a pure modulation source */',
    'wheel vibrato')

# 4. VCO1+2 CV IN: a real pitch input for both VCOs (KBD CV OUT there stays mono mode)
rep('int32_t v2cv=(inlet_vco2cv==(int32_t)0x80000000)?0:inlet_vco2cv;',
    'int32_t v2cv=(inlet_vco2cv==(int32_t)0x80000000)?0:inlet_vco2cv;int32_t v12cv=(inlet_vco12cv==(int32_t)0x80000000)?0:inlet_vco12cv; /* VCO1+2 CV IN, same scale */',
    'v12cv')
rep('+oct1+inlet_pmodpoly+vib,28);', '+oct1+inlet_pmodpoly+vib+v12cv,28);', 'q1')
rep('+oct2+inlet_pmodpoly+vib+v2cv,28);', '+oct2+inlet_pmodpoly+vib+v2cv+v12cv,28);', 'q2')
rep('+oct1+inlet_pmodarp+(int32_t)drf2+vib,28),f1a);', '+oct1+inlet_pmodarp+(int32_t)drf2+vib+v12cv,28),f1a);', 'arp vco1')
rep('+oct2+inlet_pmodarp+(int32_t)drf2+vib+v2cv,28),f);', '+oct2+inlet_pmodarp+(int32_t)drf2+vib+v2cv+v12cv,28),f);', 'arp vco2')
rep('            <int32 name="mono"/>\n            <bool32 name="memarm"/>\n         </inlets>',
    '            <int32 name="mono"/>\n            <bool32 name="memarm"/>\n            <int32 name="wheelout"/>\n            <frac32.bipolar name="vco12cv"/>\n         </inlets>',
    'voices inlets')

# patch bay: WHEEL OUT plugged, VCO1+2 CV IN source, volume at 0 = off
rep('            <int32 name="power"/>\n            <int32 name="poweron"/>\n         </outlets>',
    '            <int32 name="power"/>\n            <int32 name="poweron"/>\n            <int32 name="wheelout"/>\n            <frac32.bipolar name="vco12cv"/>\n         </outlets>',
    'patch bay outlets')
rep('int pidx[NDEST];\n', 'int pidx[NDEST];\nint wplug,sel12; /* a plug in WHEEL OUT; the source in VCO1+2 CV IN (not a parameter: 0 normal) */\n', 'patch bay decl')
rep('void patchEvent(int t,int a,int b){',
    'void setd(int d,int v){if(d==10)sel12=v;else setsel(d,v);}   /* destination 10 = VCO1+2 CV IN */\nvoid patchEvent(int t,int a,int b){',
    'setd')
rep(' if(a==b){                             /* single plug in / out */\n  int d=jack[a];',
    ' if(a==27||b==27){if(a==b)wplug=(t==4)?1:0;else if(t==4)wplug=1;}   /* WHEEL OUT: plugged (cleared by its plug-out, also in the power-on dump) */\n if(a==b){                             /* single plug in / out */\n  int d=jack[a];',
    'wheel plug')
rep('  if(d<0){if(t==5)setsel(-d-1,0);else if(!lk[a])setsel(-d-1,OPEN);}',
    '  if(d<0){if(t==5)setd(-d-1,0);else if(!lk[a])setd(-d-1,OPEN);}', 'single plug setd')
rep(' if(t==4){if(out>0)setsel(-in-1,out);}\n else setsel(-in-1,OPEN);',
    ' if(t==4){if(out>0)setd(-in-1,out);}\n else setd(-in-1,OPEN);', 'cable setd')
rep('  patchEvent(m99,m98,m6);\n }\n',
    '  patchEvent(m99,m98,m6);\n }\n if(data1==7){if(data2==0){if(pwr)patchEvent(6,0,0);}else if(!pwr)patchEvent(6,0,127);}   /* volume at 0 = off, up again = on: also when a DAW drops the iC\'s power message */\n',
    'cc7 power')
rep('for(int i=0;i<35;i++){jack[i]=0;lk[i]=0;}monof=0;pwr=1;pon=0;',
    'for(int i=0;i<35;i++){jack[i]=0;lk[i]=0;}monof=0;pwr=1;pon=0;wplug=0;sel12=0;', 'patch bay init')
rep('jack[18]=-7;  // EG1 TRIG in', 'jack[18]=-7;  // EG1 TRIG in\njack[25]=-11; // VCO1+2 CV in (KBD CV here = mono mode)', 'jack 25')
rep('//             21 initial gain, 25 VCO1+2 CV in, 28 ESP env out,', '//             21 initial gain, 28 ESP env out,', 'jack comment')
rep('outlet_mono=monof;outlet_power=pwr;outlet_poweron=pon;',
    'outlet_mono=monof;outlet_power=pwr;outlet_poweron=pon;outlet_wheelout=wplug;', 'wheelout out')
rep('outlet_evtype=ev99;outlet_evjacka=ev98;outlet_evjackb=ev6;',
    'outlet_evtype=ev99;outlet_evjacka=ev98;outlet_evjackb=ev6;\noutlet_vco12cv=(sel12>0&&sel12<=OPEN&&sel12!=11)?src[sel12]:NORMAL;   /* KBD CV in VCO1+2 CV IN is mono mode, not pitch */',
    'vco12cv out')

# 5. delay: louder wet signal at full DELAY AMOUNT (0.62 -> 0.85)
rep('float bw=mc*0.62f;', 'float bw=mc*0.85f;', 'delay wet')

# nets
rep('   </nets>',
    '      <net>\n         <source obj="PatchBay" outlet="wheelout"/>\n         <dest obj="MS20_Voices" inlet="wheelout"/>\n      </net>\n'
    '      <net>\n         <source obj="PatchBay" outlet="vco12cv"/>\n         <dest obj="MS20_Voices" inlet="vco12cv"/>\n      </net>\n   </nets>', 'nets')

open(dst, 'w', encoding='utf-8').write(s)
print('ok')
