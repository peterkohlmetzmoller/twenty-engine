import sys, math, os
sys.path.insert(0, '.')
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
D = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Cond', D + 'DejaVuSansCondensed.ttf'))
pdfmetrics.registerFont(TTFont('CondB', D + 'DejaVuSansCondensed-Bold.ttf'))
pdfmetrics.registerFont(TTFont('CondI', D + 'DejaVuSansCondensed-Oblique.ttf'))
pdfmetrics.registerFont(TTFont('CondBI', D + 'DejaVuSansCondensed-BoldOblique.ttf'))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily('Cond', normal='Cond', bold='CondB', italic='CondI', boldItalic='CondBI')

from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white, black
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                PageBreak, NextPageTemplate, Flowable, KeepTogether, CondPageBreak)
import panel as P
import esp_label as EL
import mg_waves as MG

PAGE = landscape(A4)
PWID, PHGT = PAGE
M = 16 * mm
RED = HexColor('#d8322a')
INK = HexColor('#151515')
GREY = HexColor('#777777')
LIGHT = HexColor('#f2f2f0')
RULE = HexColor('#c8c8c8')
NAME = 'TWENTY ENGINE'

ss = {}
ss['body'] = ParagraphStyle('body', fontName='Cond', fontSize=9.2, leading=12.6, textColor=INK, spaceAfter=5)
ss['small'] = ParagraphStyle('small', parent=ss['body'], fontSize=8.2, leading=10.8, spaceAfter=3)
ss['cell'] = ParagraphStyle('cell', parent=ss['body'], fontSize=8.3, leading=10.6, spaceAfter=0)
ss['cellb'] = ParagraphStyle('cellb', parent=ss['cell'], fontName='CondB')
ss['head'] = ParagraphStyle('head', parent=ss['cell'], fontName='CondB', textColor=white)
ss['h1'] = ParagraphStyle('h1', fontName='Cond', fontSize=20, leading=24, textColor=INK, spaceAfter=2)
ss['h2'] = ParagraphStyle('h2', fontName='CondB', fontSize=11.5, leading=14, textColor=INK, spaceBefore=6, spaceAfter=4)
ss['note'] = ParagraphStyle('note', parent=ss['small'], textColor=INK)
ss['cap'] = ParagraphStyle('cap', parent=ss['small'], textColor=GREY, fontSize=7.6, leading=9.6, alignment=TA_LEFT)


def p(t, st='body'): return Paragraph(t, ss[st])


class H1(Flowable):
    """Section title with a red rule and square, as on patch-book pages."""
    def __init__(self, t, num=None):
        super().__init__(); self.t = t; self.num = num
    def wrap(self, aw, ah):
        self.aw = aw; return aw, 16 * mm
    def draw(self):
        c = self.canv
        label = (f'{self.num}  ' if self.num else '') + self.t
        c.setFont('Cond', 20); c.setFillColor(INK); c.drawString(0, 6 * mm, label)
        w = max(c.stringWidth(label, 'Cond', 20) + 8 * mm, 70 * mm)
        c.setStrokeColor(RED); c.setLineWidth(0.8); c.line(0, 4.3 * mm, w, 4.3 * mm)
        c.setFillColor(RED); c.rect(0, 2.9 * mm, 1.5 * mm, 1.5 * mm, stroke=0, fill=1)


class Note(Flowable):
    def __init__(self, title, text, width=None, color=RED):
        super().__init__(); self.title = title; self.text = text; self.w = width; self.color = color
    def wrap(self, aw, ah):
        self.aw = self.w or aw
        self.para = Paragraph(f'<b>{self.title}</b>  {self.text}', ss['note'])
        _, h = self.para.wrap(self.aw - 9 * mm, ah)
        self.h = h + 6 * mm
        return self.aw, self.h
    def draw(self):
        c = self.canv
        c.setFillColor(LIGHT); c.setStrokeColor(LIGHT); c.rect(0, 0, self.aw, self.h, stroke=0, fill=1)
        c.setFillColor(self.color); c.rect(0, 0, 1.4 * mm, self.h, stroke=0, fill=1)
        self.para.drawOn(c, 5 * mm, 3 * mm)


class PanelView(Flowable):
    """Draws the panel (or a region of it) scaled to a width."""
    def __init__(self, width, region=None, maxh=None, **opts):
        super().__init__()
        self.region = region or (0, 0, P.PW, P.PH)
        x0, y0, x1, y1 = self.region
        k = width / (x1 - x0)
        if maxh and (y1 - y0) * k > maxh: k = maxh / (y1 - y0)
        self.k = k; self.w = (x1 - x0) * k; self.h = (y1 - y0) * k; self.opts = opts
    def wrap(self, aw, ah): return self.w, self.h
    def draw(self):
        c = self.canv; x0, y0, x1, y1 = self.region
        c.saveState()
        pth = c.beginPath(); pth.rect(0, 0, self.w, self.h); c.clipPath(pth, stroke=0, fill=0)
        c.scale(self.k, self.k); c.translate(-x0, -y0)
        P.draw_panel(c, **self.opts)
        c.restoreState()
        if self.region != (0, 0, P.PW, P.PH):
            c.setStrokeColor(black); c.setLineWidth(1.0); c.rect(0, 0, self.w, self.h, stroke=1, fill=0)


class Drawing(Flowable):
    def __init__(self, w, h, fn):
        super().__init__(); self.w = w; self.h = h; self.fn = fn
    def wrap(self, aw, ah): return self.w, self.h
    def draw(self): self.fn(self.canv, self.w, self.h)


def num(n):
    return f'<font name="CondB" color="#e8542a">{n}</font>'


def ctable(rows, widths, head=('', 'Control', 'CC', 'What it does')):
    data = [[Paragraph(h, ss['head']) for h in head]]
    for r in rows:
        data.append([Paragraph(str(x), ss['cellb'] if i == 1 else ss['cell']) for i, x in enumerate(r)])
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), INK),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LINEBELOW', (0, 1), (-1, -1), 0.4, RULE),
        ('TOPPADDING', (0, 0), (-1, -1), 3.2), ('BOTTOMPADDING', (0, 0), (-1, -1), 3.2),
        ('LEFTPADDING', (0, 0), (-1, -1), 4), ('RIGHTPADDING', (0, 0), (-1, -1), 4),
    ]))
    return t


def side(left, right, lw, gap=8 * mm):
    rw = PWID - 2 * M - lw - gap
    t = Table([[left, right]], colWidths=[lw + gap, rw])
    t.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0),
                           ('RIGHTPADDING', (0, 0), (-1, -1), 0), ('TOPPADDING', (0, 0), (-1, -1), 0),
                           ('BOTTOMPADDING', (0, 0), (-1, -1), 0)]))
    return t


def stack(*items):
    t = Table([[i] for i in items])
    t.setStyle(TableStyle([('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0),
                           ('TOPPADDING', (0, 0), (-1, -1), 0), ('BOTTOMPADDING', (0, 0), (-1, -1), 4)]))
    return t


FULLW = PWID - 2 * M

# ------------------------------------------------------------------ page decoration
def cover(c, doc):
    c.saveState()
    c.setFillColor(HexColor('#ececea')); c.roundRect(10 * mm, 10 * mm, PWID - 20 * mm, PHGT - 20 * mm, 9 * mm, stroke=0, fill=1)
    # title box
    c.setStrokeColor(INK); c.setLineWidth(0.9); c.setFillColor(white)
    c.roundRect(24 * mm, PHGT - 82 * mm, 112 * mm, 58 * mm, 7 * mm, stroke=1, fill=1)
    c.setFillColor(INK); c.setFont('CondB', 30); c.drawString(34 * mm, PHGT - 45 * mm, NAME)
    c.setFont('Cond', 10.5); c.drawString(34 * mm, PHGT - 52 * mm, '8-VOICE POLYPHONIC ENGINE FOR THE MS-20ic')
    c.setFont('Cond', 15); c.drawString(34 * mm, PHGT - 62 * mm, "Owner's manual")
    c.setFont('Cond', 9); c.setFillColor(GREY); c.drawString(34 * mm, PHGT - 72 * mm, 'Patch v50c  ·  Ksoloti (firmware 1.1.0+)  ·  Teensy 4.1  ·  AU + VST3')
    # instrument drawing
    w = 190 * mm; k = w / P.PW; x0 = (PWID - w) / 2; y0 = 44 * mm
    c.saveState(); c.translate(x0, y0); c.scale(k, k); P.draw_panel(c); c.restoreState()
    # keyboard + cheeks
    kh = 20 * mm
    c.setFillColor(INK); c.rect(x0 - 3 * mm, y0 - kh - 2 * mm, w + 6 * mm, kh + 2 * mm, stroke=0, fill=1)
    P.draw_keyboard(c, x0 + 18 * mm, y0 - kh - 1 * mm, w - 21 * mm, kh, 22)
    c.setFillColor(GREY); c.setFont('Cond', 7)
    c.drawRightString(PWID - 24 * mm, 15 * mm, 'MS-20 is a trademark of KORG Inc. This is an independent project, not affiliated with or endorsed by KORG.')
    c.restoreState()


def body(c, doc):
    c.saveState()
    c.setFont('Cond', 7.5); c.setFillColor(GREY)
    c.drawString(M, PHGT - 9 * mm, f"{NAME}  ·  OWNER'S MANUAL")
    c.drawRightString(PWID - M, 9 * mm, str(doc.page))
    c.drawString(M, 9 * mm, 'Panel drawings are schematic redraws for orientation.')
    c.restoreState()


# ------------------------------------------------------------------ helper drawings
def threshold_zones(c, w, h):
    cx, cy, r = w * 0.36, h * 0.40, h * 0.30
    zones = [(0, 18, 'OFF', 'MS-20 poly', '#ffffff'), (18, 42, 'ARP', 'VCO1 organ + VCO2 arp', '#f6c9bd'), (42, 64, 'LATCH', 'chord held + arp', '#e8542a')]
    for a, b, name, sub, col in zones:
        a1 = 240 - 300 * a / 64; a2 = 240 - 300 * b / 64
        c.setFillColor(HexColor(col)); c.setStrokeColor(INK); c.setLineWidth(0.6)
        c.wedge(cx - r * 1.55, cy - r * 1.55, cx + r * 1.55, cy + r * 1.55, a2, a1 - a2, stroke=1, fill=1)
        am = math.radians((a1 + a2) / 2)
        tx, ty = cx + r * 2.05 * math.cos(am), cy + r * 2.05 * math.sin(am)
        c.setFillColor(INK); c.setFont('CondB', 9); c.drawCentredString(tx, ty + 2, name)
        c.setFont('Cond', 7.2); c.drawCentredString(tx, ty - 7, sub)
    c.setFillColor(white); c.circle(cx, cy, r * 1.05, stroke=0, fill=1)
    c.saveState(); c.translate(cx, cy); c.scale(r / 3.0, r / 3.0); P.knob(c, 0, 0, 3.0, 0.15, None); c.restoreState()
    c.setFillColor(INK); c.setFont('CondB', 8.5); c.drawCentredString(cx, cy - r * 1.9, 'SIGNAL LEVEL  (CC11)')
    c.setFont('Cond', 7); c.setFillColor(GREY)
    for v, lab in ((0, '0'), (18, '18'), (42, '42'), (64, '64')):
        a = math.radians(240 - 300 * v / 64)
        c.drawCentredString(cx + r * 1.72 * math.cos(a), cy + r * 1.72 * math.sin(a) - 2.5, lab)



def esp_map(c, w, h):
    """the ESP label (as printed for the knobs) with what each knob does in each layer"""
    OR = HexColor('#d9481f')
    labw = 30 * mm; colw = (w - labw) / 5.0
    s = 4 * colw / (EL.SPAN * mm); lh = EL.H * mm * s
    lx = labw + colw / 2 - EL.knob_x(0) * mm * s; ly = h - lh
    c.saveState(); c.translate(lx, ly); c.scale(s, s); EL.draw(c); c.restoreState()
    c.setFillColor(GREY); c.setFont('Cond', 7.2)
    for k, t in enumerate(['The ESP label', '(print it from the', 'sticker sheet)']):
        c.drawString(2 * mm, ly + lh / 2 + 4 - k * 9, t)
    rows = [('ARP OFF', False, ['Delay time', 'Feedback', 'Wow / flutter', 'Delay send']),
            ('ARP OFF + switch', True, ['Delay time', 'Feedback', 'Wow / flutter', 'Delay tone']),
            ('ARP ON', False, ['Arp rate', 'Arp octaves', 'Arp order', 'Delay send']),
            ('ARP ON + switch', True, ['Delay time', 'Feedback', 'Wow / flutter', 'Delay tone'])]
    y0 = ly - 3 * mm; rh = (y0 - 5 * mm) / len(rows)
    for r, (lab, sw, cells) in enumerate(rows):
        yt = y0 - r * rh; ym = yt - rh / 2
        if r % 2 == 0:
            c.setFillColor(LIGHT); c.rect(0, yt - rh, labw, rh, stroke=0, fill=1); c.rect(labw + colw, yt - rh, colw * 4, rh, stroke=0, fill=1)
        c.setFillColor(OR if sw else INK); c.setFont('CondB', 8.3); c.drawString(2 * mm, ym - 3, lab)
        for i, txt in enumerate(cells):
            cx = labw + colw * (i + 1.5)
            c.setFillColor(OR if sw else INK); c.setFont('CondB' if not sw else 'Cond', 9.2); c.drawCentredString(cx, ym - 3, txt)
    # SIGNAL LEVEL spans every row: the play mode
    cx = labw + colw * 0.5; x0 = labw + 2 * mm; x1 = labw + colw - 2 * mm
    c.setStrokeColor(INK); c.setLineWidth(0.8); c.setFillColor(white)
    c.roundRect(x0, 4 * mm, x1 - x0, y0 - 4 * mm, 2.5 * mm, stroke=1, fill=1)
    c.setFillColor(INK); c.setFont('CondB', 10); c.drawCentredString(cx, y0 / 2 + 9, 'PLAY MODE')
    c.setFont('Cond', 8.6)
    for k, t in enumerate(['left: off (poly)', 'about 9-1 o\'clock: arp', 'right: latch']):
        c.drawCentredString(cx, y0 / 2 - 3 - k * 10.5, t)
    c.setStrokeColor(RULE); c.setLineWidth(0.5); c.line(0, y0 + 1.5 * mm, w, y0 + 1.5 * mm)
    c.setFillColor(GREY); c.setFont('Cond', 7.2)
    c.drawString(2 * mm, 0, '"switch" = hold the momentary switch, or strike a key hard (velocity 125-127)')

def legend_jacks(c, w, h):
    items = [('out', 'Source (output) the engine provides'), ('in', 'Input the engine listens to'),
             ('mono', 'Gesture jack (mono mode / tap tempo)'), ('off', 'Not used by this engine (yet)')]
    x = 0
    for st, lab in items:
        c.saveState(); c.translate(x + 3 * mm, h / 2); c.scale(1.6, 1.6); P.jack(c, 0, 0, None, st, r=1.5); c.restoreState()
        c.setFillColor(INK); c.setFont('Cond', 8.2); c.drawString(x + 7.5 * mm, h / 2 - 3, lab)
        x += 7.5 * mm + c.stringWidth(lab, 'Cond', 8.2) + 9 * mm


def controllers(c, w, h):
    # mod wheel + momentary switch, simple line art
    c.setStrokeColor(INK); c.setLineWidth(0.8); c.setFillColor(white)
    x, y = 6 * mm, 4 * mm
    c.roundRect(x, y, 12 * mm, 26 * mm, 2 * mm, stroke=1, fill=1)
    for i in range(9):
        yy = y + 3 * mm + i * 2.5 * mm
        c.setLineWidth(0.4); c.line(x + 2 * mm, yy, x + 10 * mm, yy)
    c.setFont('CondB', 8); c.setFillColor(INK); c.drawCentredString(x + 6 * mm, y - 3.5 * mm, 'WHEEL')
    x2 = 30 * mm
    c.setLineWidth(0.8); c.roundRect(x2, y + 7 * mm, 14 * mm, 12 * mm, 1.5 * mm, stroke=1, fill=1)
    c.roundRect(x2 + 2.5 * mm, y + 9.5 * mm, 9 * mm, 7 * mm, 1 * mm, stroke=1, fill=0)
    c.drawCentredString(x2 + 7 * mm, y - 3.5 * mm, 'MOMENTARY SWITCH')


# ------------------------------------------------------------------ content
story = [NextPageTemplate('body'), PageBreak()]

# ---------------- 1. welcome
story += [H1('Welcome', '1')]
intro_l = [
    p(f'<b>{NAME}</b> turns the MS-20ic, a USB controller built in the shape of an MS-20, into a real eight-voice polyphonic synthesizer. '
      'The sound engine runs on a Ksoloti Core or a Teensy 4.1 board fitted inside the MS-20ic (see the separate build guide), and every knob, rotary switch, patch cable and LED on the panel is part of it. '
      'Play it like an MS-20 and you get chords; turn one knob and it becomes an arpeggiator, a chord-latch machine or a classic monosynth.'),
    p('Each voice has two oscillators (VCO 1 and VCO 2) with a little analog drift. They feed an MS-20-style high-pass and low-pass filter pair with soft saturation, '
      'two envelopes (EG 1 and EG 2), a modulation generator (MG) that morphs from triangle to square to smooth random, sample and hold, and a VCA. '
      'After the voice section comes a bucket-brigade style delay with wow, flutter and runaway feedback, then a limiter.'),
    p('The five knobs of the External Signal Processor (ESP) are re-used as a control surface for the delay and the arpeggiator. '
      'The keys double as 36 sound memories (hold the momentary switch and tap a key), and the panel LEDs show the MG rate, the envelope gate, the arp tempo and the memory state.'),
    p('Over USB everything you play on the MS-20ic is also passed on to your computer (MIDI thru), so you can record it or play plugins with it while the synth plays.'),
    p('<b>Ksoloti:</b> needs firmware 1.1.0 or newer (section 18). <b>Teensy 4.1:</b> one firmware file, loaded with the Teensy Loader (section 14c).'),
    Spacer(1, 3),
    p('<b>About this manual</b>', 'body'),
    p('Controls are numbered on the drawings in orange (1 to 36). LEDs are lettered A to E. Every control is listed with the MIDI CC the MS-20ic sends for it, '
      'which is also what you would automate in the plugin edition. "Editor-only" means a setting that exists in the patch but has no knob on the panel; '
      'those are collected in section 15.'),
]
intro_r = [
    p('<b>Quick start</b>', 'body'),
    p('1.  Turn the VOLUME knob out of its POWER OFF position.'),
    p('2.  At the same moment the MS-20ic sends the position of every knob, rotary and cable. The engine takes all of them, so the sound always matches what you see.'),
    p('3.  Turn SIGNAL LEVEL (32) fully left. That is plain polyphonic mode. Play some chords.'),
    p('4.  Open the filters: LPF CUTOFF (17) up, HPF CUTOFF (13) down. Add PEAK for the MS-20 scream.'),
    p('5.  Turn THRESHOLD LEVEL (36) up for delay; LOW CUT FREQ (33) sets the delay time.'),
    p('6.  Turn SIGNAL LEVEL to about 9 o\'clock for the arpeggiator, or past 12 o\'clock to latch chords.'),
    p('7.  Patch a cable from KBD CV OUT to VCO1+2 CV IN for mono mode. Pull it out to go back to poly.'),
    p('8.  Hold the momentary switch and tap a key: one of 36 stored sounds. Top C takes you back to the knobs.'),
    Spacer(1, 4),
    Note('Tip', 'If the sound starts to fuzz on clean waveforms, the mixer is driving the saturation. Lower VCO 1 / VCO 2 LEVEL a little; the filter and VCA stages are gain-staged for levels around the middle.'),
]
story.append(side(stack(*intro_l), stack(*intro_r), FULLW * 0.52))
story.append(PageBreak())

# ---------------- 2. overview
story += [H1('Panel overview', '2'), PanelView(FULLW * 0.74, callouts=True, ledcallouts=True), Spacer(1, 4)]
ovcap = p('The knob section (left) is the synthesizer itself. The patch panel (right) routes modulation between modules; the jacks marked in section 12 are live in this engine. '
            'The lower right block is the External Signal Processor: its jacks are not used, but its five knobs and two LEDs are re-assigned to the delay and arpeggiator. '
            'Knob pointers on all drawings show the patch\'s built-in starting values; the real knobs take over as soon as the MS-20ic is switched on.', 'small')
ov = [[num(1) + '–' + num(4), 'VCO 1, portamento', '3'], [num(5) + '–' + num(8), 'VCO 2, master tune', '3'], [num(9) + '–' + num(12), 'Mixer, frequency modulation', '4'],
      [num(13) + '–' + num(20), 'High-pass and low-pass filters', '5'], [num(21) + '–' + num(22), 'Modulation generator (MG)', '6'], [num(23) + '–' + num(30), 'EG 1 and EG 2', '7'],
      [num(31), 'Volume and power switch', '8'], [num(32) + '–' + num(36), 'ESP knobs: delay and arpeggiator', '8'],
      ['Keys', 'Patch memory (36 sounds)', '11'], ['Jacks', 'Patch panel', '12'], ['A–E', 'LEDs', '13'], ['USB', 'Computer (MIDI thru)', '14']]
t = ctable(ov, [26 * mm, 70 * mm, 18 * mm], head=('No.', 'Area', 'Section'))
story += [side(ovcap, t, FULLW * 0.5), PageBreak()]

# ---------------- 3. VCOs
reg_vco = (0, 0, 25.4, P.PH)
story += [H1('VCO 1, VCO 2, portamento and tune', '3')]
rows = [
    [num(1), 'VCO 1 WAVEFORM', '77', 'Four shapes, each changed by PULSE WIDTH (2):<br/>'
     '<b>Triangle position</b>: a soft sine that PULSE WIDTH morphs toward triangle.<br/>'
     '<b>Sawtooth</b>: saw through a 12-stage phaser; PULSE WIDTH sets the phaser depth and the MG sweeps it.<br/>'
     '<b>Pulse</b>: square wave; PULSE WIDTH narrows it (PWM).<br/><b>Noise</b>: per-voice noise, pitched by the filter.'],
    [num(2), 'PULSE WIDTH', '14', 'Shape control for VCO 1, see above. Fully left = pure sine, pure saw or 50% square.'],
    [num(3), 'VCO 1 SCALE', '15', "32', 16', 8', 4'. VCO 1 reaches one octave lower than VCO 2."],
    [num(4), 'PORTAMENTO', '5', '<b>Poly</b>: glide by my custom-made <i>Multibend Like A Pedal Steel Guitar</i> algorithm. Play legato within two semitones and the held note bends to the new pitch, like a pedal-steel string; '
     'soft key = slow bend (up to 2.5 s at full), hard key = fast. Fully left = off.<br/><b>Mono</b>: classic portamento; at half way a glide takes about 0.9 s.'],
    [num(5), 'VCO 2 WAVEFORM', '82', 'Four shapes. <b>Note:</b> the engine order is triangle, sawtooth, square, ring modulator, so the first three positions are one step off the panel symbols (see the box below).'],
    [num(6), 'PITCH', '19', 'Detunes VCO 2 smoothly by up to 5 semitones down or up. The middle of the knob is a small in-tune zone, and just outside it the detune is very fine (a few cents) for slow analog beating. The ends are exactly −5 and +5 semitones.'],
    [num(7), 'VCO 2 SCALE', '22', "16', 8', 4', 2'."],
    [num(8), 'MASTER TUNE', '18', 'Fine-tunes the whole instrument by up to one semitone down or up. Centre = A 440.'],
]
right = stack(ctable(rows, [9 * mm, 30 * mm, 10 * mm, None]),
              Note('VCO 2 waveform labels', 'Position 1 (saw symbol) plays a triangle, position 2 (square) plays a sawtooth, position 3 (narrow pulse) plays a square, '
                   'position 4 is the ring modulator (VCO 1 × VCO 2). Reordering to match the panel is a one-line change for a later update.'))
pv = PanelView(52 * mm, reg_vco, callouts=True, maxh=150 * mm); story.append(side(pv, right, pv.w))
story.append(Spacer(1, 4))
story.append(p('<b>Voices.</b> Eight voices, each with its own VCO 1 and VCO 2 and a slow, slightly different pitch drift so chords breathe like analog. '
               'The mod wheel adds vibrato to all voices: 4.6 Hz, up to ±50 cents at full.', 'small'))
story.append(PageBreak())

# ---------------- 4. mixer + FM
story += [H1('VCO mixer and frequency modulation', '4')]
rows = [
    [num(9), 'VCO 1 LEVEL', '20', 'Level of VCO 1 into the filters. In arp and latch mode this is the level of the held chord (the organ part).'],
    [num(10), 'VCO 2 LEVEL', '21', 'Level of VCO 2. In arp and latch mode this is the level of the arpeggio.'],
    [num(11), 'MG/T.EXT', '12', 'Pitch modulation from the MG (vibrato, trills, random pitch). If a cable is patched into TOTAL, this knob scales the patched signal instead of the MG.'],
    [num(12), 'EG1/EXT', '93', 'Pitch sweep from EG 1, upward, up to +24 semitones at full. If a cable is patched into FREQ, this knob scales that signal instead of EG 1.'],
]
right = stack(ctable(rows, [9 * mm, 28 * mm, 10 * mm, None]),
              p('<b>Gain staging.</b> The mixer feeds a saturating filter like on the original. Around the middle of the knobs the sound is clean; '
                'toward full the filter starts to overdrive, which is useful for leads and basses but will fuzz up pure sines and triangles.', 'small'),
              p('<b>Arp and latch.</b> The mixer becomes a balance control: VCO 1 LEVEL for the held chord, VCO 2 LEVEL for the arpeggio. '
                'In mono mode with the arp on, both VCOs play the arpeggio.', 'small'))
pv = PanelView(30 * mm, (25.4, 0, 38.8, P.PH), callouts=True, maxh=150 * mm); story.append(side(pv, right, pv.w))
story.append(PageBreak())

# ---------------- 5. filters
story += [H1('High-pass and low-pass filters', '5')]
rows = [
    [num(13), 'HPF CUTOFF FREQUENCY', '28', 'High-pass cutoff. Fully left lets everything through.'],
    [num(14), 'HPF PEAK', '29', 'High-pass resonance. Near full it rings and self-oscillates.'],
    [num(15), 'HPF MG/T.EXT', '30', 'MG (or the TOTAL jack) modulating the HPF cutoff.'],
    [num(16), 'HPF EG2/EXT', '31', 'EG 2 sweeping the HPF cutoff. A cable in HPF CUTOFF replaces EG 2 as the source.'],
    [num(17), 'LPF CUTOFF FREQUENCY', '74', 'Low-pass cutoff.'],
    [num(18), 'LPF PEAK', '71', 'Low-pass resonance, from gentle to screaming self-oscillation.'],
    [num(19), 'LPF MG/T.EXT', '85', 'MG (or the TOTAL jack) modulating the LPF cutoff.'],
    [num(20), 'LPF EG2/EXT', '79', 'EG 2 sweeping the LPF cutoff. A cable in LPF CUTOFF replaces EG 2 as the source.'],
]
right = stack(ctable(rows, [9 * mm, 36 * mm, 10 * mm, None]),
              p('The signal runs VCO mixer → HPF → LPF → VCA, as on the MS-20, with soft saturation in the filter path. '
                'The filter sweep is shared by the voices: each new note restarts it. The filter for the played voices runs at twice the sample rate (96 kHz) inside, '
                'for a smoother, cleaner top end and resonance.', 'small'),
              Note('Filters only sound while you play', 'With PEAK at full the filters self-oscillate, but only while notes are held, in poly and mono mode. '
                   'When you let go of the keys the filter goes quiet with the release, so the instrument never howls on its own.'))
pv = PanelView(56 * mm, (38.8, 0, 63.6, P.PH), callouts=True, maxh=150 * mm); story.append(side(pv, right, pv.w))
story.append(PageBreak())

# ---------------- 6. MG
story += [H1('Modulation generator (MG)', '6')]
rows = [
    [num(21), 'WAVEFORM', '76', 'Morphs the shape continuously: triangle (left) → square (middle) → smooth random (right). '
     'The patch-panel outputs MG TRI OUT and MG SQ OUT always give triangle and square.'],
    [num(22), 'FREQUENCY', '27', 'MG rate, from very slow sweeps to audio-like trills. The RATE LED (A) blinks with the MG square wave.'],
]
right = stack(ctable(rows, [9 * mm, 24 * mm, 10 * mm, None]),
              p('The MG goes to pitch (11), HPF (15) and LPF (19). On VCO 1\'s sawtooth it also sweeps the phaser. '
                'The MG square is the normal clock for sample and hold (section 12).', 'small'),
              Spacer(1, 8), p('<b>WAVEFORM (21) from left to right</b>. The shape blends smoothly between the three; the drawings are made from the engine\'s own MG.', 'small'),
              Spacer(1, 3), Drawing(FULLW - 42 * mm, 50 * mm, MG.draw))
pv = PanelView(30 * mm, (63.6, 0, 76.0, P.PH - 33.5), callouts=True, ledcallouts=True, maxh=150 * mm); story.append(side(pv, right, pv.w))
story.append(PageBreak())

# ---------------- 7. EGs
story += [H1('Envelope generators EG 1 and EG 2', '7')]
rows = [
    [num(23), 'EG 1 DELAY', '24', 'Wait before EG 1 starts after a key goes down.'],
    [num(24), 'EG 1 ATTACK', '23', 'EG 1 rise time.'],
    [num(25), 'EG 1 RELEASE', '26', 'EG 1 fall time after the key is released.'],
    [num(26), 'EG 2 HOLD', '25', 'Holds notes for a moment after you let go (up to about 1.4 s). <b>Turned all the way up</b> it latches chords: released notes keep sounding until you play the next chord (notes within 60 ms count as one chord). Turn it down and the held chord releases.'],
    [num(27), 'EG 2 ATTACK', '73', 'Rise time of loudness and filter sweep.'],
    [num(28), 'EG 2 DECAY', '75', 'Fall time from the peak to the sustain level.'],
    [num(29), 'EG 2 SUSTAIN', '70', 'Level held while the key is down.'],
    [num(30), 'EG 2 RELEASE', '72', 'Fade-out after the key is released.'],
]
right = stack(ctable(rows, [9 * mm, 26 * mm, 10 * mm, None]),
              p('<b>EG 1</b> drives pitch through EG1/EXT (12). Its gate shows on the EG LED (B). In arp and latch mode it shapes only the arpeggiated VCO 2.', 'small'),
              p('<b>EG 2</b> shapes the loudness of each voice and sweeps the filters through the two EG2/EXT knobs (16, 20). '
                'Releasing a key during the attack does not let the attack finish: the envelope goes to HOLD and then RELEASE straight away. '
                'In arp and latch mode EG 2 shapes the arpeggio; the held chord on VCO 1 is gated like an organ.', 'small'))
pv = PanelView(90 * mm, (63.6, 0, P.LW, P.PH), callouts=True, ledcallouts=True, maxh=150 * mm); story.append(side(pv, right, pv.w))
story.append(PageBreak())

# ---------------- 8. Volume + ESP
story += [H1('Volume and the ESP knobs', '8')]
story.append(p(f'<b>{num(31)} VOLUME (CC7)</b> sets the master level. Its POWER OFF switch darkens the panel; switching on re-syncs every control. '
               'The five ESP knobs work in two layers. The normal layer depends on the arp mode; holding the <b>momentary switch</b> (or striking a key at velocity 125–127) '
               'selects the second layer. A knob takes effect the moment it moves.'))
rows = [
    [num(32), 'SIGNAL LEVEL', '11', 'Play mode: off · arp · latch (section 10)', '=', '=', '='],
    [num(33), 'LOW CUT FREQ', '88', 'Delay time', 'Delay time', 'Arp rate', 'Delay time'],
    [num(34), 'HIGH CUT FREQ', '89', 'Feedback', 'Feedback', 'Arp octaves (1–4)', 'Feedback'],
    [num(35), 'CV ADJUST', '90', 'Wow / flutter', 'Wow / flutter', 'Arp order', 'Wow / flutter'],
    [num(36), 'THRESHOLD LEVEL', '91', 'Delay send (dry/wet)', 'Delay tone', 'Delay send', 'Delay tone'],
]
story.append(Drawing(FULLW, 112 * mm, esp_map))
story.append(Spacer(1, 4))
story.append(Spacer(1, 3))
story.append(p('Arp order (CV ADJUST, four zones): up, down, up/down, random. Arp octaves (HIGH CUT, four zones): 1, 2, 3, 4.', 'small'))
story.append(PageBreak())

# ---------------- 9. switch, keyboard
story += [H1('Momentary switch, wheel and keyboard', '9')]
left = stack(Drawing(60 * mm, 36 * mm, controllers))
right = stack(
    p('<b>Momentary switch: hold.</b> Two things happen while you hold it: the keys go silent and become memory keys (section 11), '
      'and the ESP knobs switch to their second layer (delay tone, time, feedback and wow/flutter). Let go and everything returns.'),
    p('<b>Tap tempo (with a cable).</b> Patch <b>SWITCH → ESP SIGNAL IN</b> and the switch becomes a tap-tempo button instead. With the arp off, the time between two taps becomes the delay time. '
      'With the arp on, taps are quarter notes and the arp steps in sixteenths. Turning LOW CUT FREQ (delay time or arp rate) afterwards overrides the tapped value. '
      'Pull the cable to get patch memory back.'),
    p('<b>Hard keys.</b> A note played at velocity 125–127 selects the second ESP layer while that note sounds.'),
    p('<b>Mod wheel</b> (CC1): vibrato on all voices, 4.6 Hz, up to ±50 cents. The wheel is also available as a patch-panel source (WHEEL jack).'),
    p('<b>Keyboard.</b> Eight-voice polyphony in poly mode. Velocity sets the glide speed of the <i>Multibend Like A Pedal Steel Guitar</i> algorithm (section 3). In mono mode: lowest note wins, single trigger, classic portamento.'),
    Note('Knob feel', 'When you move a knob the engine glides to the new value quickly and lands with a tiny random offset, so a knob never lands in exactly the same place twice. '
         'It is very subtle, but it keeps repeated tweaks alive, like on an analog panel.'))
story.append(side(left, right, 64 * mm))
story.append(PageBreak())

# ---------------- 10. play modes
story += [H1('Play modes: poly, arp, latch and mono', '10')]
left = stack(Spacer(1, 7 * mm), Drawing(92 * mm, 70 * mm, threshold_zones))
right = stack(
    p('SIGNAL LEVEL (32) selects the play mode. The PEAK LED (C) blinks the arp tempo whenever the arp is on.'),
    p('<b>Off: MS-20 poly.</b> Every key gets its own voice with VCO 1 and VCO 2, EG 2 shapes every voice, and the filters sweep with each new note.'),
    p('<b>Arp.</b> VCO 1 plays the held keys as a sustained organ chord (gated, no envelope), while VCO 2 arpeggiates the same notes, shaped by EG 1 and EG 2. '
      'Balance the two with the mixer. ESP knobs: rate (LOW CUT), octaves (HIGH CUT), order (CV ADJUST). The delay locks to a dotted eighth of the arp tempo.'),
    p('<b>Latch.</b> As arp, but the chord keeps playing after you let go. Play a new chord and it replaces the old one; notes struck within 60 ms count as one chord, '
      'so chords never pile up on top of each other.'),
    p('<b>Mono mode</b> (any SIGNAL LEVEL position): patch <b>KBD CV OUT → VCO1+2 CV IN</b>. One voice, VCO 1 and VCO 2 together, lowest-note priority, single trigger '
      '(legato does not restart the envelopes) and classic portamento. With the arp on, both VCOs play the arpeggio. Pull either plug to return to poly.'),
)
story.append(side(left, right, 92 * mm))
story.append(PageBreak())

# ---------------- delay (still section 10b)
story += [H1('The delay', '10b')]
rows = [
    ['Send', 'THRESHOLD LEVEL (normal layer)', 'How much of the synth goes into the delay. The dry signal always stays at full.'],
    ['Time', 'LOW CUT FREQ (arp off) or switch + LOW CUT FREQ (arp on), or tap tempo (cable)', 'Delay time. In arp mode the delay follows the arp at a dotted eighth unless you set the time yourself.'],
    ['Feedback', 'HIGH CUT FREQ', 'Number of repeats. Above roughly 90% the delay runs away: repeats build up into self-oscillation, ideal for dub throws. Pull the knob back to tame it.'],
    ['Wow / flutter', 'CV ADJUST', 'Pitch wobble of the repeats, from steady to worn tape.'],
    ['Tone', 'switch + THRESHOLD LEVEL', 'Darkens or brightens the repeats (starts in the middle).'],
]
story.append(ctable(rows, [26 * mm, 70 * mm, None], head=('Function', 'Knob', 'What it does')))
story.append(Spacer(1, 4))
story.append(p('The delay is a single-tap bucket-brigade model with its own filtering and saturation, followed by a limiter at −1 dBFS so runaway feedback stays under control. '
               'Stereo spread and echo width are set in the editor (section 15).', 'small'))
story.append(PageBreak())


# ---------------- 11. patch memory
def keymap(c, w, h):
    nk = 22; kw = w / nk; kh = h * 0.62; y0 = h * 0.22
    c.setStrokeColor(INK); c.setLineWidth(0.6)
    zones = [(0, 7, '#e8e8e4', 'BASSES'), (7, 14, '#f6d9cf', 'PADS & KEYS'), (14, 21, '#e8e8e4', 'LEADS & EFFECTS')]
    for a, b, col, lab in zones:
        c.setFillColor(HexColor(col)); c.rect(a * kw, y0, (b - a) * kw, kh, stroke=0, fill=1)
        c.setFillColor(INK); c.setFont('CondB', 8.5); c.drawCentredString((a + b) / 2 * kw, y0 + kh + 5, lab)
    c.setFillColor(HexColor('#e8542a')); c.rect(21 * kw, y0, kw, kh, stroke=0, fill=1)
    c.setFillColor(INK); c.setFont('CondB', 8.5); c.drawCentredString(21.5 * kw, y0 + kh + 5, 'PANEL')
    for i in range(nk):
        c.rect(i * kw, y0, kw, kh, stroke=1, fill=0)
    pat = [1, 1, 0, 1, 1, 1, 0]
    for i in range(nk - 1):
        if pat[i - 7 * (i // 7)]:
            c.setFillColor(INK); c.rect((i + 1) * kw - kw * 0.3, y0 + kh * 0.38, kw * 0.6, kh * 0.62, stroke=0, fill=1)
    c.setFillColor(GREY); c.setFont('Cond', 7.5)
    c.drawString(0, y0 - 11, 'lowest C = sound 1')
    c.drawRightString(w, y0 - 11, 'top C = back to the knobs')

story += [H1('Patch memory: 36 sounds on the keys', '11')]
story.append(Drawing(FULLW, 34 * mm, keymap))
story.append(Spacer(1, 4))
rows = [
    ['Recall a sound', 'Hold the momentary switch, tap a key, let go of the key.', 'All LEDs blink once'],
    ['Save a sound', 'Hold the momentary switch, hold a key for 2 seconds.', 'TRIG blinks fast; all LEDs blink 5 times once the SD card has it'],
    ['Back to the knobs', 'Hold the momentary switch, tap the top C.', 'All LEDs blink once'],
    ['Empty key', 'Nothing changes.', 'EG LED blinks twice'],
    ['SD card problem', 'A save could not be written, or no readable card at power-on.', 'PEAK and TRIG blink 8 times'],
]
story.append(ctable(rows, [34 * mm, 120 * mm, None], head=('What', 'How', 'You see')))
story.append(Spacer(1, 5))
left = stack(
    p('<b>While the switch is held the keys are silent.</b> They only choose sounds. Notes you were already holding keep sounding. Let go of the switch and the keyboard plays normally again.'),
    p('<b>Knobs after a recall.</b> The panel no longer matches the sound, so the knobs don\'t jump. Turning a knob moves the value from where the stored sound left it, '
      'scaled so that knob and value reach the end of the range together. From then on the knob works directly again. A one-step wobble of a knob is ignored.'),
    p('<b>The TRIG LED</b> blinks slowly while you are on a stored sound, and stops when you go back to the knobs (top C) or switch the MS-20ic off and on.'))
right = stack(
    p('<b>What a memory stores:</b> VCO 1 and VCO 2 (waveform, scale, pulse width, pitch), portamento, the mixer, both filters with their modulation, EG 1, EG 2 and the MG.'),
    p('<b>What it doesn\'t store:</b> the delay, the arpeggiator settings, master tune and volume. They stay on their knobs, so a recall never changes your tempo, echoes or level.'),
    p('<b>Saved for good.</b> Sounds are written to the SD card in the background (no click) and are there after a power cycle. Delete <font name="CondB">/MS20MEM.BIN</font> from the card to get the factory sounds back.'),
    p('<b>PEAK and TRIG blink 8 times?</b> The card is missing, not pushed in all the way, or can\'t be read. The sound still plays until you switch off, but it isn\'t stored. Push the card in until it clicks and save again.'),
    Note('Tap tempo or memory', 'With a cable from SWITCH to ESP SIGNAL IN the switch is a tap-tempo button and patch memory is off. Pull the cable and memory is back.'))
story.append(side(left, right, FULLW * 0.48))
story.append(PageBreak())

story += [H1('Factory sounds', '11b')]
story.append(p('These are the sounds on the keys when you first start. Overwrite any of them with your own. Keys are counted from the lowest C.'))
FN = ['Punch Saw Bass', 'Acid Reso', 'Sub Pulse', 'Wide Pulse Bass', 'Octave Bass', 'Fifth Bass', 'Ring Bass', 'Sine Thump', 'Round Bass', 'Wobble', 'Glide Bass', 'Phaser Bass', 'Poly Strings', 'Brass', 'Organ', 'Soft Keys', 'Bell', 'Glass Pad', 'Sweep Pad', 'Phaser Pad', 'Clav', 'Harpsi', 'Vowel Pad', 'Poly Synth', 'Fourth Lead', 'Square Lead', 'Screamer', 'Soft Flute', 'Reed', 'Theremin', 'Rising Zap', 'Laser', 'Random Bleeps', 'Wind', 'Ring Metal', 'Slow Swell']
nn = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
octs = ['Low', 'Middle', 'High']
cols = []
for o in range(3):
    rows = [[f'{nn[i]}', FN[o * 12 + i]] for i in range(12)]
    cols.append(ctable(rows, [12 * mm, 52 * mm], head=('Key', octs[o] + ' octave')))
tt = Table([cols], colWidths=[FULLW / 3] * 3)
tt.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0)]))
story.append(tt)
story.append(Spacer(1, 4))
story.append(p('Low octave: basses. Middle octave: pads and keys. High octave: leads and effects. The top C is not a sound: it takes you back to the knobs.', 'small'))
story.append(PageBreak())

# ---------------- 11. patch bay
story += [H1('Patch panel', '12')]
story.append(PanelView(FULLW * 0.78, (P.LW, P.RY(770) - 0.3, P.PW, P.PH), colorjacks=True, jacknums=True))
story.append(Spacer(1, 3))
story.append(Drawing(FULLW, 9 * mm, legend_jacks))
story.append(p('The MS-20ic reports every cable it sees, so patching works like on the MS-20: plug a source into an input and it takes over from the normal connection. '
               'Small grey numbers are the MS-20ic\'s own jack numbers (see the MIDI section).', 'small'))
story.append(PageBreak())

story += [H1('Patch panel: what each jack does', '12b')]
src = [
    ['MG TRI OUT', 'MG triangle'], ['MG SQ OUT', 'MG square'], ['EG1 OUT', 'EG 1 envelope'], ['EG1 REV OUT', 'EG 1, inverted'],
    ['EG2 REV OUT', 'EG 2, inverted'], ['S&amp;H OUT', 'Sample and hold'], ['PINK / WHITE', 'Noise'], ['WHEEL', 'Mod wheel'],
    ['SWITCH', 'Momentary switch (on/off)'], ['KBD CV OUT', 'Keyboard pitch'], ['KBD TRIG OUT', 'Keyboard gate'],
]
dst = [
    ['TOTAL', 'Replaces the MG on all three MG/T.EXT knobs (11, 15, 19).'],
    ['FREQ', 'Replaces EG 1 on EG1/EXT (12), the pitch sweep.'],
    ['VCO2 CV IN', 'Added to VCO 2 pitch only.'],
    ['HPF CUTOFF', 'Replaces EG 2 on the HPF EG2/EXT knob (16).'],
    ['LPF CUTOFF', 'Replaces EG 2 on the LPF EG2/EXT knob (20).'],
    ['VCA CTRL IN', 'Controls the output level (0 to full). Unpatched = full.'],
    ['EG1 TRIG IN', 'Replaces the keyboard gate for EG 1.'],
    ['KBD TRIG IN', 'Replaces the keyboard gate for EG 2.'],
    ['S&amp;H IN', 'Signal to sample. Normal: noise.'],
    ['S&amp;H CLOCK', 'When to sample. Normal: MG square.'],
    ['VCO1+2 CV IN', 'Mono-mode gesture: KBD CV OUT into this jack = mono mode.'],
    ['ESP SIGNAL IN', 'Tap-tempo gesture: SWITCH into this jack = tap tempo (memory off).'],
]
lt = ctable(src, [32 * mm, 52 * mm], head=('Source jack', 'Signal'))
rt = ctable(dst, [32 * mm, None], head=('Input jack', 'Effect when a cable is plugged in'))
story.append(side(stack(lt, Spacer(1, 4),
                        p('<b>Plugging only one end</b> of a cable into an input breaks the normal connection with nothing patched, as on the original, so a source is muted.', 'small'),
                        p('<b>Ideas:</b> EG1 REV OUT → TOTAL for vibrato that fades in; S&amp;H OUT → LPF CUTOFF for random filter steps on the MG clock; '
                          'WHEEL → FREQ for a manual pitch bend; SWITCH → EG1 TRIG IN to fire pitch sweeps by hand.', 'small')),
                  stack(rt, Spacer(1, 4),
                        p('<b>Not used yet:</b> EXT SIGNAL IN, INITIAL GAIN, the auxiliary VCA (VCA IN / VCA OUT), SIGNAL OUT and PHONES jacks, and the ESP jacks other than SIGNAL IN. '
                          'Plugging into them does nothing.', 'small')), 90 * mm))
story.append(PageBreak())

# ---------------- 12. LEDs
story += [H1('LEDs and power-on', '13')]
rows = [
    ['A', 'RATE', 'Blinks with the MG square wave, all the time.'],
    ['B', 'EG', 'Follows the EG 1 gate: on while a key (or an arp step) is held.'],
    ['C', 'PEAK', 'Blinks the arp tempo whenever the arp is on; free-running when idle, locked to the steps while notes play.'],
    ['D', 'TRIG', 'Patch memory: lit while the switch is held, fast blink while a key is held to save, slow blink while you are on a stored sound.'],
    ['E', 'VOLUME', 'On while the panel is switched on.'],
]
story.append(ctable(rows, [12 * mm, 26 * mm, None], head=('LED', 'Name', 'Meaning')))
story.append(Spacer(1, 6))
story.append(p('<b>Power.</b> Switching off with the VOLUME knob darkens the panel; switching on lights it again.'))
story.append(p('<b>Blink messages.</b> All LEDs blink once = sound recalled. All LEDs blink five times = sound saved. The EG LED blinks twice = that key has no stored sound. '
               'PEAK and TRIG blink eight times = the sound could not be saved (Ksoloti: SD card missing, loose or unreadable).'))
story.append(p('<b>Sync.</b> At power-on the MS-20ic sends the position of every rotary switch, the state of every jack and every knob in about 4 ms. '
               'The engine applies them all, so the sound matches the panel. Cables already plugged in at power-on are reported in the same burst.'))
story.append(PageBreak())


# ---------------- 14. Computer (MIDI thru)
story += [H1('Computer (MIDI thru)', '14')]
left = stack(
    p('<b>MIDI to your computer.</b> Connect the board\'s USB port (Ksoloti or Teensy) to your computer and everything you play on the MS-20ic (keys, knobs, wheel, switch, cables) is also sent there. '
      'Record it in your DAW or play plugins with it, while the synth keeps playing. It is always on; there is nothing to switch.'),
    p('<b>Port name.</b> Your computer sees the MS-20ic through the board inside it: the MIDI port is called "Ksoloti" on the Ksoloti edition and "Twenty Engine" on the Teensy edition. Choose that port in your software.'),
    p('<b>No computer?</b> No problem: the messages are simply dropped, and nothing slows down.'),
    p('<b>Memory keys.</b> The keys you press for patch memory while the switch is held are sent as CC 102 (key down) and CC 103 (key up), with the key number as the value, '
      'so your DAW does not play them as notes.'),
    p('<b>From the computer.</b> On the Teensy edition, notes and CCs from the computer also play the engine, and SysEx is passed on to the MS-20ic. On the Ksoloti edition, messages from the computer are not passed back to the MS-20ic.'))
right = stack(
    Note('Computer stopped receiving?', 'If MIDI from the Ksoloti suddenly stops arriving (for example after many patch uploads, or after the computer slept), '
         'switch the MS-20ic off and on again. The synth itself is not affected.'),
    Spacer(1, 4),
    p('<b>Plugin edition.</b> The AU and VST3 edition runs the same engine on a computer with a stock MS-20ic as the controller (section 14b). Its window shows "Connected" and which controller it found '
      '(MS-20ic, Ksoloti Core or Teensy).', 'small'))
story.append(side(left, right, FULLW * 0.52))
story.append(PageBreak())

# ---------------- 14b. plugin edition
story += [H1('Plugin edition (AU and VST3)', '14b')]
left = stack(
    p('The same engine runs as a plugin on your computer, played from a <b>stock MS-20ic</b> over USB. No Ksoloti and no opening the MS-20ic. '
      'Every knob, rotary, patch cable and LED works as described in this manual.'),
    p('<b>Formats.</b> Audio Unit (Logic, MainStage, GarageBand) and VST3 (Ableton Live, Bitwig, Reaper, Cubase and others). '
      'Mac: Intel and Apple Silicon, macOS 10.13 (High Sierra) or newer. Windows: 64-bit VST3.'),
    p('<b>Install on a Mac.</b> Open <font name="CondB">TwentyEngine-Installer.pkg</font>. The first time, macOS blocks it because the beta is not signed by Apple: '
      'open <b>System Settings &gt; Privacy &amp; Security</b>, scroll down and click <b>Open Anyway</b>. The installer puts the AU and the VST3 in place '
      '(you can untick either), then restart your music software.'),
    p('<b>Install on Windows.</b> Run <font name="CondB">TwentyEngine-Setup.exe</font>. If Windows shows "Windows protected your PC", click <b>More info &gt; Run anyway</b> '
      '(the beta is not code-signed yet). Then rescan plugins. Uninstall from <b>Settings &gt; Apps</b>; your memory sounds stay.'),
    p('<b>Memory.</b> The 36 sounds live in one file, shared by all projects: Mac <font name="CondB">~/Library/Application Support/Twenty Engine/MS20MEM.BIN</font>, '
      'Windows <font name="CondB">%APPDATA%\\Twenty Engine\\MS20MEM.BIN</font>.', 'small'))
right = stack(
    Note('Why there is no graphical interface',
         'Twenty Engine has no knobs on the screen, on purpose. Its interface is the MS-20ic itself. Sound should be made with your hands and ears, '
         'not by staring at a screen and clicking at pictures of knobs. Turn a real knob, patch a real cable, and listen.'),
    Spacer(1, 4),
    p('There are already plenty of MS-20 plugins with beautiful graphics, and they work well. This is not another one. '
      'Twenty Engine exists to make the MS-20ic a real instrument: the controller you already own becomes the synth.'),
    p('The plugin window tells you one thing only: whether your controller is connected. Everything else happens on the panel. '
      'Your DAW can still show and automate every parameter if you need it.'))
story.append(side(left, right, FULLW * 0.52))
story.append(PageBreak())

# ---------------- 14c. Teensy edition
story += [H1('Teensy 4.1 edition', '14c')]
left = stack(
    p('The same engine also runs on a <b>Teensy 4.1 with the PJRC audio shield</b>, fitted inside the MS-20ic instead of a Ksoloti. '
      'Every knob, cable, LED and memory key works as described in this manual. How to wire it is in the separate build guide.'),
    p('<b>Install.</b> Download the free <b>Teensy Loader</b> from pjrc.com. Connect the Teensy\'s USB port to your computer, open '
      '<font name="CondB">TwentyEngineTeensy.hex</font> with <b>File &gt; Open HEX File</b>, and press the small button on the Teensy once. '
      'The Teensy Loader programs it and restarts it. That is all: no Arduino, no SD card, no firmware update.'),
    p('<b>Memory.</b> The 36 sounds are saved in the Teensy\'s own flash and survive power-off. No SD card is needed. If a card is in the Teensy\'s slot, '
      'it gets a backup copy (<font name="CondB">MS20MEM.BIN</font>), and a card holding a file from a Ksoloti is read once when the flash is still empty, so you can bring your sounds over.'),
    p('<b>Audio.</b> Line out and headphones on the audio shield, 48 kHz, dithered to 16 bit. Latency is very low: the engine works in 16-sample blocks (0.33 ms).'),
    p('<b>USB.</b> The Teensy\'s own USB port shows up on your computer as a MIDI device called <b>Twenty Engine</b> (section 14).'))
right = stack(
    Note('HQ sound', 'The Teensy has power to spare, so its engine runs a little finer than the Ksoloti patch: '
         'the oscillators run at twice the sample rate (96 kHz) for cleaner high notes with less aliasing, and every voice drifts slightly in pitch and filter, '
         'like the voice cards of a vintage poly. Otherwise it is the same engine and the same sound memories.'),
    Spacer(1, 4),
    p('<b>Processor.</b> About 33 % at the worst case (eight voices, arp, every effect). Normal playing stays well below that.', 'small'),
    p('<b>Saving.</b> All LEDs blink five times = saved. PEAK and TRIG blink eight times = the save failed.', 'small'),
    p('<b>Updating.</b> A new version is a new .hex file, loaded the same way. Your stored sounds normally stay in flash; an SD card in the slot keeps a backup copy to be safe.', 'small'))
story.append(side(left, right, FULLW * 0.52))
story.append(PageBreak())

# ---------------- 13. editor-only
story += [H1('Editor-only settings', '15')]
story.append(p('These parameters exist in the patch (Ksoloti editor, and as automatable parameters in the plugin edition) but have no knob on the MS-20ic. Values are in the editor\'s 0–64 scale.'))
rows = [
    ['VCO 2 fine tune', 'Voices', 'Fine detune of VCO 2 against VCO 1, below one semitone.'],
    ['Multi-trigger', 'Voices', 'Restart the shared envelopes on every new note in poly mode.'],
    ['Arp octaves / order', 'Voices', 'Start values for the arp (3 octaves, up/down). The ESP knobs override them.'],
    ['Ext level', 'Mixer', 'Level of the external input (reserved; EXT SIGNAL IN is not routed yet).'],
    ['Click / noise', 'Filter', 'Key click and a touch of noise added after the filter, for analog character. Default 6.'],
    ['Bass boost', 'VCA', 'Low-end lift at the output, up to about +8 dB. Default 63.5.'],
    ['Drive', 'VCA', 'Output saturation. Default 37.'],
    ['Delay spread', 'Delay', 'Stereo offset of the echoes: left later, right earlier, 0–20 ms. Default 0.'],
    ['Delay width', 'Delay', 'Stereo width of the echoes only (dry stays mono): 0 = mono, 32 = 100%, 64 = 200%. Default 64.'],
    ['Patch-panel selectors', 'Patch bay', 'One selector per input showing its source. Cables set them; editing by hand overrides until the next cable event.'],
]
story.append(ctable(rows, [38 * mm, 22 * mm, None], head=('Setting', 'Module', 'What it does')))
story.append(PageBreak())

# ---------------- 14. MIDI
story += [H1('MIDI implementation', '16')]
cc = [('5', 'Portamento'), ('7', 'Volume'), ('11', 'Signal level (play mode)'), ('12', 'Freq mod MG/T.EXT'), ('14', 'Pulse width'), ('15', 'VCO 1 scale'),
      ('18', 'Master tune'), ('19', 'VCO 2 pitch'), ('20', 'VCO 1 level'), ('21', 'VCO 2 level'), ('22', 'VCO 2 scale'), ('23', 'EG 1 attack'),
      ('24', 'EG 1 delay'), ('25', 'EG 2 hold'), ('26', 'EG 1 release'), ('27', 'MG frequency'), ('28', 'HPF cutoff'), ('29', 'HPF peak'),
      ('30', 'HPF MG/T.EXT'), ('31', 'HPF EG2/EXT'), ('70', 'EG 2 sustain'), ('71', 'LPF peak'), ('72', 'EG 2 release'), ('73', 'EG 2 attack'),
      ('74', 'LPF cutoff'), ('75', 'EG 2 decay'), ('76', 'MG waveform'), ('77', 'VCO 1 waveform'), ('79', 'LPF EG2/EXT'), ('82', 'VCO 2 waveform'),
      ('85', 'LPF MG/T.EXT'), ('88', 'Low cut (delay time)'), ('89', 'High cut (feedback)'), ('90', 'CV adjust (wow)'), ('91', 'Threshold (delay send)'), ('93', 'Freq mod EG1/EXT'), ('1', 'Mod wheel')]
cc.sort(key=lambda x: int(x[0]))
half = (len(cc) + 1) // 2
data = [['CC', 'Control', 'CC', 'Control']]
for i in range(half):
    a = cc[i]; b = cc[i + half] if i + half < len(cc) else ('', '')
    data.append([a[0], a[1], b[0], b[1]])
t = Table([[Paragraph(x, ss['head'] if r == 0 else ss['cell']) for x in row] for r, row in enumerate(data)], colWidths=[11 * mm, 36 * mm, 11 * mm, 36 * mm])
t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, 0), INK), ('LINEBELOW', (0, 1), (-1, -1), 0.4, RULE),
                       ('TOPPADDING', (0, 0), (-1, -1), 1.6), ('BOTTOMPADDING', (0, 0), (-1, -1), 1.6)]))
right = stack(
    p('<b>Channel.</b> Everything on MIDI channel 1. Rotary switches send 0, 43, 85 or 127. The momentary switch sends channel pressure.'),
    p('<b>Patch cables.</b> Each cable event is a CC99 / CC98 / CC6 triple: CC99 = event type, CC98 and CC6 = the two jack numbers. '
      'Type 4 = connect (or plug in), type 5 = disconnect (or pull out); the same jack number twice means a single plug. '
      'Type 6 is the power switch: 6/0/127 = on, 6/0/0 = off.'),
    p('<b>Jack numbers</b> (as on the patch-panel drawing): 0 TOTAL, 1 MG TRI OUT, 2 S&amp;H IN, 3 ESP SIGNAL IN, 4 S&amp;H CLOCK, 5 FREQ, 6 MG SQ OUT, 7 S&amp;H OUT, '
      '8 AMP OUT, 9 EXT SIGNAL IN, 10 EG1 OUT, 11 EG1 REV OUT, 12 VCA IN, 13 HPF CUTOFF, 14 VCA CTRL IN, 15 BPF OUT, 16 VCA OUT, 17 LPF CUTOFF, '
      '18 EG1 TRIG IN, 19 PINK, 20 F-V OUT, 21 INITIAL GAIN, 22 EG2 REV OUT, 23 WHITE, 24 VCO2 CV IN, 25 VCO1+2 CV IN, 26 KBD TRIG IN, 27 WHEEL, '
      '28 ENV OUT, 29 SIGNAL OUT, 30 PHONES, 31 KBD CV OUT, 32 KBD TRIG OUT, 33 SWITCH, 34 ESP TRIG OUT.', 'small'),
    p('<b>LEDs.</b> SysEx <font name="CondB">F0 42 40 6E 01 01 [led] [3F on / 1F off] F7</font>; led 0 RATE, 1 EG, 2 PEAK, 3 TRIG, 4 VOLUME.'),
    p('<b>Power-on dump.</b> Power state, rotary positions, one "pull out" event for each of the 35 jacks, then every knob.'),
    p('<b>Memory file.</b> Ksoloti: stored sounds live in <font name="CondB">/MS20MEM.BIN</font> on the SD card; delete it to get the factory sounds back. '
      'Teensy: in the Teensy\'s own flash, with a backup copy of the same file on an SD card if one is fitted.'),
)
story.append(side(t, right, 94 * mm))
story.append(PageBreak())

# ---------------- 15. notes
story += [H1('Notes and known limits', '17')]
story.append(p('<b>VCO 2 waveform order.</b> The engine plays triangle, sawtooth, square, ring modulator on the four VCO 2 positions, while the panel shows sawtooth, square, narrow pulse, ring.'))
story.append(p('<b>VCA CTRL IN versus INITIAL GAIN.</b> On the MS-20 the main VCA is controlled through INITIAL GAIN, and the jack above the small VCA is the auxiliary VCA\'s control input. '
               'In this engine that jack (14) controls the main output level and INITIAL GAIN is unused. This may change.'))
story.append(p('<b>Patch panel status.</b> The jack map is confirmed on the MS-20ic; behaviour with many real cables at once, and power-on with cables already in, is still being tested.'))
story.append(p('<b>Reserved jacks.</b> An auxiliary VCA, arp outputs on the ESP jacks, VCO1+2 CV IN beyond the mono gesture and a chord gate on KBD TRIG IN are planned, depending on a later hardware decision.'))
story.append(p('<b>Firmware.</b> The Ksoloti patch needs Ksoloti firmware 1.1.0 or newer. Section 18 shows the update. The Teensy edition needs no firmware update: the .hex file is everything (section 14c).'))
story.append(p('<b>Processor headroom.</b> With all eight voices on the highest notes, every effect and the arp running, the Ksoloti peaks around 86 % DSP load and the Teensy around 33 %. '
               'Normal playing stays well below that. Watch the DSP Load meter in the editor if you change the patch.'))
story.append(p('<b>Plugin edition.</b> The AU and VST3 edition runs the same engine on a computer with a stock MS-20ic as its controller. All parameters in this manual are automatable in the host. '
               'Because an instrument plugin cannot send MIDI to hardware itself, the plugin talks to the MS-20ic\'s LEDs through its own MIDI port; its window only shows whether the MS-20ic is connected.'))

story.append(PageBreak())
# ---------------- 18. installing / updating
story += [H1('Installing and updating to Ksoloti 1.1.0', '18')]
left = stack(
    p('Twenty Engine needs the Ksoloti Patcher and firmware <b>1.1.0</b> (or newer). Coming from 1.0.12, the update goes through <b>Rescue Mode</b> once. It takes about 10 minutes.'),
    p('<b>1. Download.</b> Get <b>Ksoloti 1.1.0</b> from <a href="https://github.com/ksoloti/ksoloti/releases/tag/1.1.0" color="#c2410c"><u>github.com/ksoloti/ksoloti/releases</u></a>. It is marked "Pre-release", so it sits above "Latest". Pick the file for your computer.'),
    p('<b>2. Mac only.</b> Drag the Ksoloti app into your Applications folder and start it from there (not from Downloads).'),
    p('<b>3. Rescue Mode.</b> Unplug the Ksoloti. Hold the small button <b>S1</b> on the board while you plug the USB cable back in. Both LEDs stay off: that is right. '
      'Inside the MS-20ic you may need to open the case to reach S1.'),
    p('<b>4. Windows only.</b> In the Patcher choose <b>Board &gt; Select Device...</b>. If "STM DFU Bootloader" is not listed, install the driver with Zadig as shown on the Ksoloti rescue page: <a href="https://ksoloti.github.io/3-4-rescue_mode.html" color="#c2410c"><u>ksoloti.github.io/3-4-rescue_mode.html</u></a>'),
    p('<b>5. Check.</b> <b>Board &gt; Select Device...</b> should list "STM DFU BOOTLOADER". Close the window with Cancel (the greyed-out OK is normal).'),
    p('<b>6. Flash.</b> Choose <b>Board &gt; Firmware &gt; Flash (Rescue)</b>. It takes a few minutes. <b>Don\'t unplug</b> while it runs.'),
    p('<b>7. Done.</b> The board restarts by itself. Wait until only the green LED is lit, then connect as usual.'))
right = stack(
    p('<b>Load Twenty Engine</b>'),
    p('1.  Open <font name="CondB">TwentyEngine_Ksoloti_v50c.axp</font> in the Patcher and press <b>Live</b>.'),
    p('2.  To make it start by itself: <b>Patch &gt; Upload to SD Card as Startup</b>, with the SD card pushed fully in.'),
    p('3.  Switch the MS-20ic off and on. The synth starts without a computer.'),
    Spacer(1, 4),
    Note('Going back', 'To return to 1.0.12 later, do the same rescue flash from the 1.0.12 Patcher. Patches made for 1.1.0, like this one, do not run on 1.0.12.'),
    Spacer(1, 4),
    p('Full rescue instructions from the Ksoloti team: <a href="https://ksoloti.github.io/3-4-rescue_mode.html" color="#c2410c"><u>ksoloti.github.io/3-4-rescue_mode.html</u></a>', 'small'))
story.append(side(left, right, FULLW * 0.54))
story.append(PageBreak())

# ---------------- 19. license and credits
story += [H1('License and credits', '19')]
left = stack(
    p('<b>Free software under the GNU General Public License v3.</b> Twenty Engine (the Ksoloti patch, the AU/VST3 plugin and the Teensy version) is released under the GPL v3, '
      'the same license as the Axoloti and Ksoloti ecosystem it was built on. Full text: <a href="https://www.gnu.org/licenses/gpl-3.0.html" color="#c2410c"><u>gnu.org/licenses/gpl-3.0</u></a>  (<a href="https://www.tldrlegal.com/license/gnu-general-public-license-v3-gpl-3" color="#c2410c"><u>plain-language summary</u></a>).'),
    p('<b>What that means for you:</b> you may use, study, change and share Twenty Engine, also commercially. If you pass it on, changed or not, '
      'it stays under the GPL v3 and the source goes with it. The Ksoloti patch file (.axp) is itself the source code; the source of the plugin and the Teensy version is published alongside them.'),
    p('<b>No warranty.</b> Twenty Engine comes without any warranty, as described in the license. Opening your MS-20ic and fitting a board is at your own risk.'))
right = stack(
    p('<b>Built on</b>'),
    p('•  <b>Axoloti</b> by Johannes Taelman, and <b>Ksoloti</b> by the Ksoloti team: board, firmware and patcher (GPL v3).', 'small'),
    p('•  <b>iPlug2</b> (plugin framework, WDL-style license) and the <b>Steinberg VST 3 SDK</b> (MIT).', 'small'),
    p('•  <b>Roboto</b> font in the plugin window (Apache License 2.0).', 'small'),
    p('•  <b>PJRC Teensy</b> core and audio libraries (MIT-style).', 'small'),
    Spacer(1, 4),
    p('MS-20 is a trademark of KORG Inc. Twenty Engine is an independent project, not affiliated with or endorsed by KORG.', 'small'))
story.append(side(left, right, FULLW * 0.54))

doc = BaseDocTemplate(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TwentyEngine_Manual.pdf'), pagesize=PAGE,
                      leftMargin=M, rightMargin=M, topMargin=14 * mm, bottomMargin=14 * mm,
                      title=f"{NAME} owner's manual", author='Pete')
fr = Frame(M, 14 * mm, PWID - 2 * M, PHGT - 28 * mm, id='f', leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
doc.addPageTemplates([PageTemplate('cover', [fr], onPage=cover), PageTemplate('body', [fr], onPage=body)])
doc.build(story)
print('ok')
