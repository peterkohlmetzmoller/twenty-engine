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
                           ('RIGHTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (0, 0), gap), ('TOPPADDING', (0, 0), (-1, -1), 0),
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
    rows = [('ARP OFF', False, ['Delay time', 'Feedback', 'Wow / flutter', 'Delay amount']),
            ('ARP OFF + switch', True, ['Delay time', 'Feedback', 'Wow / flutter', 'Delay tone']),
            ('ARP ON', False, ['Arp rate', 'Arp octaves', 'Arp order', 'Delay amount']),
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



# ------------------------------------------------------------------ new helpers
OR = HexColor('#d9481f')
ss['ct'] = ParagraphStyle('ct', parent=ss['body'], fontName='CondB', fontSize=10.5, leading=13, spaceAfter=2)
ss['cb'] = ParagraphStyle('cb', parent=ss['small'], fontSize=8.4, leading=11.0, spaceAfter=0)
ss['lead'] = ParagraphStyle('lead', parent=ss['body'], fontSize=11, leading=15, spaceAfter=6)


class Card(Flowable):
    """light box with an orange top bar: a title and a short text"""
    def __init__(self, title, text, w, h, tag=None):
        super().__init__(); self.title = title; self.text = text; self.w = w; self.h = h; self.tag = tag
    def wrap(self, aw, ah): return self.w, self.h
    def draw(self):
        c = self.canv
        c.setFillColor(LIGHT); c.rect(0, 0, self.w, self.h, stroke=0, fill=1)
        c.setFillColor(OR); c.rect(0, self.h - 1.2 * mm, self.w, 1.2 * mm, stroke=0, fill=1)
        if self.tag:
            c.setFillColor(GREY); c.setFont('Cond', 7.2); c.drawRightString(self.w - 3 * mm, self.h - 6 * mm, self.tag)
        t = Paragraph(self.title, ss['ct']); tw, th = t.wrap(self.w - 6 * mm - (14 * mm if self.tag else 0), self.h)
        t.drawOn(c, 3 * mm, self.h - 3.2 * mm - th)
        b = Paragraph(self.text, ss['cb']); bw, bh = b.wrap(self.w - 6 * mm, self.h)
        b.drawOn(c, 3 * mm, self.h - 5.2 * mm - th - bh)


def cards(items, cols, h, gap=4 * mm, width=None):
    width = width or FULLW
    w = (width - gap * (cols - 1)) / cols
    rows = [items[i:i + cols] for i in range(0, len(items), cols)]
    data = [[Card(*it, w=w, h=h) if len(it) == 2 else Card(it[0], it[1], w, h, it[2]) for it in r] + [''] * (cols - len(r)) for r in rows]
    t = Table(data, colWidths=[w + gap] * (cols - 1) + [w], rowHeights=[h + gap] * len(rows))
    t.setStyle(TableStyle([('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0),
                           ('TOPPADDING', (0, 0), (-1, -1), 0), ('BOTTOMPADDING', (0, 0), (-1, -1), 0), ('VALIGN', (0, 0), (-1, -1), 'TOP')]))
    return t


ss['cellL'] = ParagraphStyle('cellL', parent=ss['cell'], fontSize=10.2, leading=13.5)
ss['cellLb'] = ParagraphStyle('cellLb', parent=ss['cellL'], fontName='CondB')
ss['headL'] = ParagraphStyle('headL', parent=ss['cellL'], fontName='CondB', textColor=white)


def gtable(rows, widths, head, big=False):
    """plain two/three column table, first column bold"""
    hs, bs, cs = ('headL', 'cellLb', 'cellL') if big else ('head', 'cellb', 'cell')
    data = [[Paragraph(h, ss[hs]) for h in head]]
    for r in rows:
        data.append([Paragraph(r[0], ss[bs])] + [Paragraph(x, ss[cs]) for x in r[1:]])
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, 0), INK), ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                           ('LINEBELOW', (0, 1), (-1, -1), 0.4, RULE),
                           ('TOPPADDING', (0, 0), (-1, -1), 5 if big else 2.6), ('BOTTOMPADDING', (0, 0), (-1, -1), 5 if big else 2.6)]))
    return t


# ================================================================== the manual
story = [NextPageTemplate('body'), PageBreak()]

# ---------------- 1. what's new
story += [H1('What Twenty Engine adds')]
story.append(p('You know the MS-20. Every knob, switch and patch cable on the MS-20ic does what it says on the panel. '
               'This manual only covers what is new: the polyphony, the play modes, the arpeggiator and delay on the ESP knobs, the sound memories, '
               'the new waveforms and the shortcuts that tie it all together.', 'lead'))
story.append(Spacer(1, 2))
story.append(cards([
    ('Eight voices', 'Real polyphony. Every voice has its own VCO 1, VCO 2, filters and envelopes, and drifts a little on its own, like an analog poly.', 'p. 5'),
    ('Arp and latch', 'SIGNAL LEVEL picks poly, arpeggio or latch. In arp mode VCO 1 holds the chord like an organ while VCO 2 arpeggiates it.', 'p. 4'),
    ('Mono by cable', 'Patch KBD CV OUT into VCO1+2 CV IN and it is a classic monosynth with portamento. Pull the cable for poly.', 'p. 5'),
    ('Delay', 'A bucket-brigade delay with wow, flutter and runaway feedback, on the ESP knobs.', 'p. 5'),
    ('Multibend glide', 'My custom-made <i>Multibend Like A Pedal Steel Guitar</i> algorithm: in poly, held notes bend to the new pitch like pedal-steel strings.', 'p. 6'),
    ('New waveforms', 'VCO 1 gets a sine, a phaser saw, PWM and pitched noise. The MG morphs from triangle to square to smooth random.', 'p. 6'),
    ('36 sounds on the keys', 'Hold the switch and tap a key to recall a sound; hold the key to save. Basses, pads and leads are already there.', 'p. 7'),
    ('MIDI to your computer', 'Everything you play goes out over USB, so you can record it or play plugins with it while the synth plays.', 'p. 9'),
], 4, 30 * mm))
story.append(Spacer(1, 2))
story.append(p('Twenty Engine runs inside the MS-20ic on a <b>Ksoloti Core</b> or a <b>Teensy 4.1</b> (see the build guide), or as an <b>AU/VST3 plugin</b> played from a stock MS-20ic. '
               'It works the same on all three. Recording with the plugin is on p. 10.', 'small'))
story.append(PageBreak())

# ---------------- 2. shortcuts
story += [H1('Shortcuts')]
story.append(p('Everything new is reached from the panel. No menus, no screen. These are all the shortcuts.', 'lead'))
rows = [
    ['Hold the momentary switch + tap a key', 'Recall one of the 36 sounds. The keys are silent while the switch is held.'],
    ['Hold the momentary switch + hold a key 2 s', 'Save the current sound on that key.'],
    ['Hold the momentary switch + tap top C', 'Back to the knobs: the sound follows the panel again.'],
    ['Hold the momentary switch', 'The ESP knobs change to their second layer: delay time, feedback, wow and tone.'],
    ['Strike a key very hard (velocity 125–127)', 'Same second ESP layer, while that note sounds.'],
    ['SIGNAL LEVEL left / middle / right', 'Poly / VCO2 arp / latch.'],
    ['Cable: KBD CV OUT → VCO1+2 CV IN', 'Mono mode: one voice, lowest note wins, classic portamento.'],
    ['Cable: SWITCH → ESP SIGNAL IN', 'The switch becomes a tap-tempo button for the delay or the arp. Patch memory is off while it is patched.'],
    ['EG 2 HOLD fully up', 'Chord latch in poly: released notes keep sounding until you play the next chord.'],
    ['Soft key / hard key with PORTAMENTO up', 'Slow / fast Multibend glide in poly.'],
]
story.append(gtable(rows, [86 * mm, None], ('Do this', 'To get this'), big=True))
story.append(PageBreak())

# ---------------- 3. play modes + ESP
story += [H1('Play modes and the ESP knobs')]
story.append(p('The five ESP knobs no longer process an external signal. The first picks the play mode; the other four run the delay, or the arpeggiator when it is on. '
               'Hold the momentary switch (or hit a key very hard) for the second layer. A knob takes effect the moment it moves. '
               'The label below can be printed from the sticker sheet and put around the knobs.', 'small'))
story.append(Drawing(FULLW, 112 * mm, esp_map))
story.append(Spacer(1, 4))
story.append(p('Arp order, four zones: up, down, up/down, random. Arp octaves, four zones: 1 to 4. The PEAK LED blinks the arp tempo while the arp is on.', 'small'))
story.append(PageBreak())

# ---------------- 4. poly, arp, latch, mono + delay
story += [H1('Poly, arp, latch, mono and the delay')]
left = stack(
    p('<b>Poly.</b> Eight voices. Each key gets VCO 1 and VCO 2, EG 2 shapes every voice, and the filters sweep with each new note.'),
    p('<b>VCO2 arp.</b> VCO 1 plays the held keys as a sustained organ chord, while VCO 2 arpeggiates the same notes, shaped by EG 1 and EG 2. '
      'VCO 1 LEVEL and VCO 2 LEVEL become the balance between chord and arpeggio. The delay locks to a dotted eighth of the arp tempo.'),
    p('<b>Latch.</b> As arp, but the chord keeps playing when you let go. Play a new chord to replace it; notes struck within 60 ms count as one chord.'),
    p('<b>Mono.</b> Patch KBD CV OUT into VCO1+2 CV IN, at any play mode. One voice with VCO 1 and VCO 2, lowest-note priority, single trigger and classic portamento. '
      'With the arp on, both VCOs play the arpeggio. Pull the cable to go back to poly.'),
    p('<b>Tap tempo.</b> Patch SWITCH into ESP SIGNAL IN. With the arp off, the time between two taps sets the delay time. With the arp on, taps are quarter notes and the arp steps in sixteenths. '
      'Turning LOW CUT FREQ afterwards overrides the tapped tempo.'))
right = stack(
    p('<b>The delay</b>', 'h2'),
    gtable([['Delay amount', 'THRESHOLD LEVEL', 'How much goes into the delay. The dry sound always stays at full.'],
            ['Time', 'LOW CUT FREQ', 'With the arp on: hold the switch, or let it follow the arp.'],
            ['Feedback', 'HIGH CUT FREQ', 'Above about 90 % it runs away into self-oscillation. Pull back to tame it.'],
            ['Wow / flutter', 'CV ADJUST', 'From steady to worn tape.'],
            ['Tone', 'switch + THRESHOLD', 'Darker or brighter repeats.']],
           [24 * mm, 30 * mm, None], ('', 'Knob', '')),
    Spacer(1, 4),
    p('A limiter after the delay keeps runaway feedback under control.', 'small'))
story.append(side(left, right, FULLW * 0.47))
story.append(PageBreak())

# ---------------- 5. sound additions
story += [H1('New sounds on familiar knobs')]
left = stack(
    p('<b>VCO 1 WAVEFORM + PULSE WIDTH.</b> PULSE WIDTH now shapes every VCO 1 waveform:'),
    gtable([['Triangle', 'A soft sine; PULSE WIDTH morphs it toward triangle.'],
            ['Sawtooth', 'Saw through a 12-stage phaser. PULSE WIDTH sets the depth, the MG sweeps it.'],
            ['Pulse', 'Square; PULSE WIDTH narrows it (PWM).'],
            ['Noise', 'Noise per voice, pitched by the filter.']], [22 * mm, None], ('Position', 'Sound')),
    Spacer(1, 3),
    p('<b>VCO 2 WAVEFORM.</b> The engine order is triangle, sawtooth, square, ring modulator: the first three are one step off the panel symbols.', 'small'),
    p('<b>VCO 2 PITCH.</b> Small in-tune zone in the middle; just outside it the detune is a few cents, for slow analog beating. The ends are exactly ±5 semitones.', 'small'),
    p('<b>PORTAMENTO in poly.</b> My custom-made <i>Multibend Like A Pedal Steel Guitar</i> algorithm: play legato within two semitones and the held note bends to the new pitch. '
      'Soft key = slow bend (up to 2.5 s), hard key = fast. In mono it is classic portamento.', 'small'),
    p('<b>Filters.</b> With PEAK at full they self-oscillate only while notes are held, so the instrument never howls on its own.', 'small'))
right = stack(
    p('<b>MG WAVEFORM</b> morphs from triangle through square to smooth random. MG TRI OUT and MG SQ OUT on the patch panel always give triangle and square.', 'small'),
    Drawing(FULLW * 0.5, 48 * mm, MG.draw),
    p('<b>Knob feel.</b> Knobs glide to a new value and land with a tiny random offset, so repeated tweaks stay alive, like on an analog panel.', 'small'))
story.append(side(left, right, FULLW * 0.45))
story.append(PageBreak())

# ---------------- 6. memory
story += [H1('36 sounds on the keys')]
story.append(Drawing(FULLW, 30 * mm, keymap))
rows = [
    ['Recall', 'Hold the switch, tap a key.', 'All LEDs blink once'],
    ['Save', 'Hold the switch, hold a key for 2 seconds.', 'TRIG blinks fast, then all LEDs blink 5 times'],
    ['Back to the knobs', 'Hold the switch, tap the top C.', 'All LEDs blink once'],
    ['Empty key', 'Nothing changes.', 'EG LED blinks twice'],
    ['Save failed', 'Ksoloti: SD card missing or loose. Push it in and save again.', 'PEAK and TRIG blink 8 times'],
]
story.append(gtable(rows, [30 * mm, 105 * mm, None], ('', 'How', 'You see')))
story.append(Spacer(1, 5))
story.append(side(
    stack(p('<b>Stored:</b> VCOs, portamento, mixer, both filters, EG 1, EG 2 and the MG. '
            '<b>Not stored:</b> delay, arp, master tune and volume, so a recall never changes your tempo or echoes.', 'small'),
          p('<b>After a recall</b> the knobs don\'t jump. A knob moves the value from where the sound left it and catches up by the end of its travel. '
            'TRIG blinks slowly while you are on a stored sound.', 'small')),
    stack(p('<b>Where.</b> Ksoloti: <font name="CondB">MS20MEM.BIN</font> on the SD card (delete it for the factory sounds). '
            'Teensy: its own flash, with a backup on an SD card if one is fitted. Plugin: one file shared by all projects.', 'small'),
          p('With the tap-tempo cable patched, memory is off.', 'small')), FULLW * 0.5))
story.append(PageBreak())

# ---------------- 7. factory sounds + patch panel
story += [H1('Factory sounds and the patch panel')]
nn = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
FN = ['Punch Saw Bass', 'Acid Reso', 'Sub Pulse', 'Wide Pulse Bass', 'Octave Bass', 'Fifth Bass', 'Ring Bass', 'Sine Thump', 'Round Bass', 'Wobble', 'Glide Bass', 'Phaser Bass',
      'Poly Strings', 'Brass', 'Organ', 'Soft Keys', 'Bell', 'Glass Pad', 'Sweep Pad', 'Phaser Pad', 'Clav', 'Harpsi', 'Vowel Pad', 'Poly Synth',
      'Fourth Lead', 'Square Lead', 'Screamer', 'Soft Flute', 'Reed', 'Theremin', 'Rising Zap', 'Laser', 'Random Bleeps', 'Wind', 'Ring Metal', 'Slow Swell']
cols = []
for o, name in enumerate(['Low: basses', 'Middle: pads, keys', 'High: leads, FX']):
    cols.append(gtable([[nn[i], FN[o * 12 + i]] for i in range(12)], [9 * mm, 30 * mm], ('', name)))
tt = Table([cols], colWidths=[42 * mm] * 3)
tt.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0)]))
jacks = gtable([
    ['TOTAL', 'Replaces the MG on the three MG/T.EXT knobs.'],
    ['FREQ', 'Replaces EG 1 on EG1/EXT (pitch sweep).'],
    ['VCO2 CV IN', 'Added to VCO 2 pitch.'],
    ['HPF / LPF CUTOFF', 'Replaces EG 2 on that filter\'s EG2/EXT knob.'],
    ['VCA CTRL IN', 'Output level.'],
    ['EG1 / KBD TRIG IN', 'Replaces the keyboard gate for EG 1 / EG 2.'],
    ['S&amp;H IN / CLOCK', 'Signal to sample (noise) / when (MG square).'],
    ['VCO1+2 CV IN', 'KBD CV OUT here = mono mode.'],
    ['ESP SIGNAL IN', 'SWITCH here = tap tempo.'],
], [32 * mm, None], ('Input', 'What a cable does'))
story.append(side(stack(tt, Spacer(1, 3), p('The top C is not a sound: it takes you back to the knobs. Overwrite any key with your own sound.', 'cap')),
                  stack(jacks, Spacer(1, 3),
                        p('Outputs: MG TRI, MG SQ, EG1, EG1 REV, EG2 REV, S&amp;H, PINK, WHITE, WHEEL, SWITCH, KBD CV, KBD TRIG. '
                          'Not used yet: EXT SIGNAL IN, INITIAL GAIN, VCA IN/OUT, SIGNAL OUT, PHONES and the other ESP jacks.', 'small')), 132 * mm))
story.append(PageBreak())

# ---------------- 8. editions, USB, setup
story += [H1('Editions, USB and setup')]
story.append(cards([
    ('Ksoloti Core', 'Inside the MS-20ic. Install the Ksoloti Patcher 1.1.0 or newer, update the board\'s firmware, open '
     '<font name="CondB">TwentyEngine_Ksoloti_v50c.axp</font> and choose Patch &gt; Upload to SD Card as Startup. '
     'Sounds are saved on the SD card.'),
    ('Teensy 4.1', 'Inside the MS-20ic, with the PJRC audio shield. Open <font name="CondB">TwentyEngineTeensy.hex</font> in the Teensy Loader (included in the download) and press the button on the Teensy. '
     'Sounds are saved in its own flash; no SD card needed. Oscillators run at 96 kHz and each voice drifts slightly, for an extra analog feel. Very low latency.'),
    ('AU / VST3 plugin', 'On your computer, played from a stock MS-20ic over USB. Mac (macOS 10.13 or newer, Intel and Apple Silicon) and Windows. '
     'The installer adds Twenty Engine Instrument, which records your playing like audio (p. 10). No graphics on purpose: the MS-20ic is the interface.'),
], 3, 40 * mm))
story.append(Spacer(1, 3))
story.append(side(
    stack(p('<b>MIDI to your computer</b>', 'h2'),
          p('Connect the board\'s USB port and everything you play on the MS-20ic is also sent to your computer. The port is called "Ksoloti" or "Twenty Engine" (Teensy). '
            'Memory keys are sent as CC 102 / 103, so your DAW doesn\'t play them as notes. On the Teensy, notes from the computer also play the engine.', 'small')),
    stack(p('<b>New CCs</b>', 'h2'),
          gtable([['11', 'SIGNAL LEVEL: play mode'], ['88', 'LOW CUT: delay time / arp rate'], ['89', 'HIGH CUT: feedback / arp octaves'],
                  ['90', 'CV ADJUST: wow / arp order'], ['91', 'THRESHOLD: delay amount / tone'], ['102 / 103', 'Memory key down / up']],
                 [18 * mm, None], ('CC', 'Function'))), FULLW * 0.5))
story.append(Spacer(1, 3))
story.append(p('<b>The plugin and your panel.</b> The MS-20ic only sends where its knobs are when you switch it on with the volume knob. '
               'So the plugin remembers the last panel it saw and starts from it when it connects, with the start-up light show; '
               'the plugin window says <font name="CondB">Panel from memory</font>. Moved knobs while no plugin was open? '
               'Switch the volume off and on: the window then says <font name="CondB">Panel read from the MS-20ic</font>.', 'small'))
story.append(Spacer(1, 3))
story.append(p('<b>Installing the plugin.</b> Mac: if macOS says it cannot check the installer for malicious software, open '
               'System Settings &gt; Privacy &amp; Security and click <b>Open Anyway</b>. Windows: if "Windows protected your PC" appears, '
               'click <b>More info</b> &gt; <b>Run anyway</b>. Then restart your music software.', 'small'))
story.append(Spacer(1, 3))
story.append(p('Wiring and step-by-step installation are in the separate <b>build guide</b>. '
               'Twenty Engine is free software under the GNU GPL v3, built on Axoloti/Ksoloti, PJRC Teensy, iPlug2 and the VST 3 SDK. '
               'MS-20 is a trademark of KORG Inc.; this is an independent project, not affiliated with or endorsed by KORG.', 'cap'))
story.append(PageBreak())

# ---------------- 9. recording with the plugin
story += [H1('Recording with the plugin')]
story.append(p('<b>Twenty Engine Instrument</b> records your playing like audio: every take plays back exactly as you played it, '
               'knob moves included, from the sound it started with. Your panel stays yours the whole time.', 'lead'))
story.append(side(
    stack(p('<b>Record it like audio</b>', 'h2'),
          gtable([['1', 'Put Twenty Engine Instrument on an instrument track and select the MS-20ic as its MIDI input.'],
                  ['2', 'Record as usual. The region gets your notes; the plugin itself records every knob move, switch and sound recall that belongs to them.'],
                  ['3', 'Play it back: you hear the take exactly as you played it, knob moves included, from the sound you started with.'],
                  ['4', 'Meanwhile the panel stays yours. Play along live on top: the live sound follows the knobs as they are now.']],
                 [7 * mm, None], ('', 'How')),
          Spacer(1, 3),
          p('Takes are saved with the project. The plugin window counts them: <font name="CondB">Recorded performances: 12</font>; '
            'double-click that line to forget them all.', 'small')),
    stack(p('<b>Good to know</b>', 'h2'),
          p('<b>A new sound for the next take:</b> just dial it in and record. Nothing to switch, no modes: '
            'earlier takes keep their own sound, whatever you do with the knobs afterwards.', 'small'),
          p('<b>Start anywhere.</b> Start playback in the middle of a take and it picks up at the next note. Copy, loop or move a region and the take goes with its notes. '
            'Change the notes and the region plays with the panel instead, like a new part.', 'small'),
          p('<b>A take is a recording, like audio.</b> Its knob moves are not edited with the mouse: to change a part, play it again. '
            'Several parts with different sounds can share one track.', 'small'),
          p('<b>Your knob moves also land in the region as MIDI CC.</b> While the MS-20ic is connected the plugin ignores them: '
            'the take it recorded itself is what plays.', 'small')), FULLW * 0.5))
story.append(Spacer(1, 4))
story.append(p('<b>Windows.</b> To keep a take and the panel apart, the plugin opens the MS-20ic next to your DAW. Windows 11 with Windows MIDI Services allows that. '
               'On older Windows the plugin says so in its window and works like a normal MIDI instrument: knob moves go into the region as MIDI CC and play back from there. '
               'For the panel LEDs, don\'t enable the MS-20ic as a MIDI output in your DAW.', 'small'))

doc = BaseDocTemplate(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TwentyEngine_Manual.pdf'), pagesize=PAGE,
                      leftMargin=M, rightMargin=M, topMargin=14 * mm, bottomMargin=14 * mm,
                      title=f"{NAME} owner's manual", author='Pete')
fr = Frame(M, 14 * mm, PWID - 2 * M, PHGT - 28 * mm, id='f', leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
doc.addPageTemplates([PageTemplate('cover', [fr], onPage=cover), PageTemplate('body', [fr], onPage=body)])
doc.build(story)
print('ok')
