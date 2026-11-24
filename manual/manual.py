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
                                PageBreak, NextPageTemplate, Flowable, KeepTogether, CondPageBreak, Image)
IMG = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'img')

def picture(name, w):
    from PIL import Image as PI
    iw, ih = PI.open(os.path.join(IMG, name)).size
    im = Image(os.path.join(IMG, name), width=w, height=w * ih / iw)
    im.hAlign = 'CENTER'
    return im
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
    c.setFont('Cond', 9); c.setFillColor(GREY); c.drawString(34 * mm, PHGT - 72 * mm, 'Version 1.1  ·  Ksoloti  ·  Teensy 4.1  ·  AU + VST3  ·  app  ·  web')
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
# pages: 1 cover, 2 what it adds, 3 new in 1.1, 4 shortcuts, 5 play modes, 6 poly/arp/mono/delay, 7 new sounds,
# 8 memory, 9 factory sounds + patch panel, 10 first patches, 11 editions, 12 the window, 13 recording, 14 app + web
WEB = 'peterkohlmetzmoller.github.io/twenty-engine'
story = [NextPageTemplate('body'), PageBreak()]

# ---------------- 2. what Twenty Engine adds
story += [H1('What Twenty Engine adds')]
story.append(p('You know the MS-20. Every knob, switch and patch cable on the MS-20ic does what it says on the panel. '
               'This manual only covers what is new: the polyphony, the play modes, the arpeggiator and delay on the ESP knobs, the sound memories, '
               'the new waveforms and the shortcuts that tie it all together.', 'lead'))
story.append(Spacer(1, 2))
story.append(cards([
    ('Eight voices', 'Real polyphony. Every voice has its own VCO 1, VCO 2, filters and envelopes, and drifts a little on its own, like an analog poly.', 'p. 6'),
    ('Arp and latch', 'SIGNAL LEVEL picks poly, arpeggio or latch. In arp mode VCO 1 holds the chord like an organ while VCO 2 arpeggiates it.', 'p. 5'),
    ('Mono by cable', 'Patch KBD CV OUT into VCO1+2 CV IN and it is a classic monosynth with portamento. Pull the cable for poly.', 'p. 6'),
    ('Delay', 'A bucket-brigade delay with wow, flutter and runaway feedback, on the ESP knobs.', 'p. 6'),
    ('Multibend glide', 'My custom-made <i>Multibend Like A Pedal Steel Guitar</i> algorithm: in poly, held notes bend to the new pitch like pedal-steel strings.', 'p. 7'),
    ('New waveforms', 'VCO 1 gets a sine, a phaser saw, PWM and pitched noise. The MG morphs from triangle to square to smooth random.', 'p. 7'),
    ('36 sounds on the keys', 'Hold the switch and tap a key to recall a sound; hold the key to save. Basses, pads and leads are already there.', 'p. 8'),
    ('MIDI to your computer', 'Everything you play goes out over USB, so you can record it or play plugins with it while the synth plays.', 'p. 11'),
], 4, 30 * mm))
story.append(Spacer(1, 2))
story.append(p('Twenty Engine runs inside the MS-20ic on a <b>Ksoloti Core</b> or a <b>Teensy 4.1</b> (see the build guide), on your computer as '
               '<b>AU/VST3 plugins</b> and an <b>app</b>, or free in your <b>web browser</b>. It sounds the same everywhere. '
               'The editions are on p. 11, the panel window on p. 12.', 'small'))
story.append(PageBreak())

# ---------------- 3. new in 1.1
story += [H1('New in 1.1')]
story.append(p('The whole MS-20 panel on your screen, Twenty Engine without a DAW and in the browser, and a list of ideas from players. '
               'Everything here works in every edition unless it says otherwise.', 'lead'))
story.append(cards([
    ('The panel on screen', 'Twenty Engine UI: every knob, switch, patch cable and LED of the MS-20 in a window. With an MS-20ic it follows the iC; without one it is a normal synth plugin.', 'p. 12'),
    ('App and web', 'Twenty Engine as an app for Mac, Windows and Linux that saves every take as a WAV, and a free edition for Chrome and Edge.', 'p. 14'),
    ('Sounds keep their cables', 'A stored sound brings back its patch too, until you touch a cable. Switch + top C held: the cables plugged in now.', 'p. 8'),
    ('Presets and CC learn', 'Save sounds in your DAW\'s preset menu, cables included. Right-click a knob in the window to put it on any MIDI controller.', 'p. 12'),
    ('VCA and INITIAL GAIN', 'The patch panel\'s VCA works as on the MS-20, and a cable into INITIAL GAIN keeps the last note sounding.', 'p. 9'),
    ('Retrigger', 'In mono every new key can start the envelopes again, like the MS-20\'s multiple trigger. In the window and on a Teensy button.', 'p. 6'),
    ('Calmer ESP knobs', 'After a layer change a knob takes over once it reaches its value, so the delay time no longer jumps. TRIG flashes with the repeats.', 'p. 5'),
    ('Bug log', 'LOG copies what happened lately. Click the blue address and an email to support opens, ready for the log.', 'p. 12'),
], 4, 31 * mm))
story.append(Spacer(1, 2))
story.append(side(
    stack(p('<b>Also new</b>', 'h2'),
          p('• MG TRI OUT follows the MG WAVEFORM knob, as on the original MS-20.<br/>'
            '• VCO1+2 CV IN takes any source as a pitch input for both VCOs. KBD CV OUT there is still mono.<br/>'
            '• The wheel answers faster. While a cable is in WHEEL OUT it gives no vibrato: it is a pure modulation source.<br/>'
            '• Arp sync: in the window the arp can follow the DAW\'s tempo, 1/4 to 1/32 with triplets.<br/>'
            '• Teensy: any function on any button, also a function switch of its own (the Teensy guide).<br/>'
            '• Key Click explained, and softer by default (p. 7).', 'small')),
    stack(p('<b>Fixed</b>', 'h2'),
          p('• Volume off and on always reads the knobs and plays the light show, also in Ableton Live and other VST3 hosts on Windows.<br/>'
            '• The volume all the way down counts as off, even if the DAW drops the iC\'s power message.<br/>'
            '• Mono + arp: VCO 1\'s phaser saw was a plain saw.<br/>'
            '• The delay is louder at full DELAY AMOUNT. The arp tops out at about 20 steps a second, where the envelopes still follow.<br/>'
            '• Projects made with Twenty Engine 1.0 open with <b>Twenty Engine MS-20ic</b>: the same plugin under its new name.', 'small')),
    FULLW * 0.5))
story.append(PageBreak())

# ---------------- 4. shortcuts
story += [H1('Shortcuts')]
story.append(p('Everything new is reached from the panel. No menus, no screen. These are all the shortcuts.', 'lead'))
rows = [
    ['Hold the momentary switch + tap a key', 'Recall one of the 36 sounds. The keys are silent while the switch is held.'],
    ['Hold the momentary switch + hold a key 2 s', 'Save the current sound, cables included, on that key.'],
    ['Hold the momentary switch + tap top C', 'Back to the knobs: the sound follows the panel and its cables again.'],
    ['Hold the momentary switch + hold top C 2 s', 'Cables only: the recalled sound now plays with the cables plugged in.'],
    ['Hold the momentary switch', 'The ESP knobs change to their second layer: delay time, feedback, wow and tone.'],
    ['Strike a key very hard (velocity 125–127)', 'Same second ESP layer, while that note sounds.'],
    ['SIGNAL LEVEL left / middle / right', 'Poly / VCO2 arp / latch.'],
    ['Cable: KBD CV OUT → VCO1+2 CV IN', 'Mono mode: one voice, lowest note wins, classic portamento.'],
    ['Cable: SWITCH → ESP SIGNAL IN', 'The switch becomes a tap-tempo button for the delay or the arp. Patch memory is off while it is patched.'],
    ['Cable: any output → INITIAL GAIN', 'The last note keeps sounding at that output\'s level.'],
    ['EG 2 HOLD fully up', 'Chord latch in poly: released notes keep sounding until you play the next chord.'],
    ['Soft key / hard key with PORTAMENTO up', 'Slow / fast Multibend glide in poly.'],
]
story.append(gtable(rows, [86 * mm, None], ('Do this', 'To get this'), big=True))
story.append(Spacer(1, 4))
story.append(p('In the window (plugin and app), <b>SWITCH</b> in the status bar is the momentary switch: hold it with the mouse and play a key.', 'small'))
story.append(PageBreak())

# ---------------- 5. play modes + ESP
story += [H1('Play modes and the ESP knobs')]
story.append(p('The five ESP knobs no longer process an external signal. The first picks the play mode; the other four run the delay, or the arpeggiator when it is on. '
               'Hold the momentary switch (or hit a key very hard) for the second layer. '
               'The label below can be printed from the sticker sheet and put around the knobs.', 'small'))
story.append(Drawing(FULLW, 112 * mm, esp_map))
story.append(Spacer(1, 4))
story.append(p('<b>Pickup.</b> When the knobs change layer, a knob leaves the value alone until you turn it past the value it now sets; then it takes over. '
               'So the delay time no longer jumps to where the arp rate was. Switching the volume off and on reads them all at once. '
               'Arp order, four zones: up, down, up/down, random. Arp octaves, four zones: 1 to 4. The PEAK LED blinks the arp tempo while the arp is on.', 'small'))
story.append(PageBreak())

# ---------------- 6. poly, arp, latch, mono + delay
story += [H1('Poly, arp, latch, mono and the delay')]
left = stack(
    p('<b>Poly.</b> Eight voices. Each key gets VCO 1 and VCO 2, EG 2 shapes every voice, and the filters sweep with each new note.'),
    p('<b>VCO2 arp.</b> VCO 1 plays the held keys as a sustained organ chord, while VCO 2 arpeggiates the same notes, shaped by EG 1 and EG 2. '
      'VCO 1 LEVEL and VCO 2 LEVEL become the balance between chord and arpeggio. The delay locks to a dotted eighth of the arp tempo.'),
    p('<b>Latch.</b> As arp, but the chord keeps playing when you let go. Play a new chord to replace it; notes struck within 60 ms count as one chord.'),
    p('<b>Mono.</b> Patch KBD CV OUT into VCO1+2 CV IN, at any play mode. One voice with VCO 1 and VCO 2, lowest-note priority and classic portamento. '
      'With the arp on, both VCOs play the arpeggio. Pull the cable to go back to poly.'),
    p('<b>Retrigger.</b> In mono, legato notes glide on without starting the envelopes again. With retrigger on, every new key starts EG 1 and EG 2, '
      'like the MS-20\'s multiple trigger. It is <b>RETRIG</b> under PORTAMENTO TIME in the window, or a RETRIG button on a Teensy.'),
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
    p('The ESP <b>TRIG</b> LED flashes with every repeat while DELAY AMOUNT is up: you see the delay time. '
      'A limiter after the delay keeps runaway feedback under control.', 'small'),
    p('<b>Arp sync</b> (the window). <b>SYNC / DAW TEMPO</b> lets the arp follow the DAW: the RATE knob then picks 1/4, 1/8, 1/8T, 1/16, 1/16T or 1/32. '
      'While the DAW is stopped it runs at the DAW\'s tempo.', 'small'))
story.append(side(left, right, FULLW * 0.47))
story.append(PageBreak())

# ---------------- 7. sound additions
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
    p('<b>MG WAVEFORM</b> morphs from triangle through square to smooth random. MG TRI OUT on the patch panel follows it, as on the original MS-20; '
      'MG SQ OUT always gives the square.', 'small'),
    Drawing(FULLW * 0.5, 44 * mm, MG.draw),
    p('<b>The wheel</b> gives vibrato, until a cable is in WHEEL OUT: then it is a pure modulation source for that cable.', 'small'),
    p('<b>Key Click.</b> The soft click and breath of noise at the start of every note you play on the keys (never the arp). '
      'It comes after the filter, as on the MS-20, so the cutoff never hides it. 3 is standard, 0 is off, 64 is the most. '
      'Plugins: the <b>Key Click</b> parameter in your DAW (under Filters). Teensy: a setting (the Teensy guide). '
      'Ksoloti: the <b>clicknoise</b> dial in the patch\'s Filter_Macro.', 'small'),
    p('<b>Knob feel.</b> Knobs glide to a new value and land with a tiny random offset, so repeated tweaks stay alive, like on an analog panel.', 'small'))
story.append(side(left, right, FULLW * 0.45))
story.append(PageBreak())

# ---------------- 8. memory
story += [H1('36 sounds on the keys')]
story.append(Drawing(FULLW, 30 * mm, keymap))
rows = [
    ['Recall', 'Hold the switch, tap a key.', 'All LEDs blink once'],
    ['Save', 'Hold the switch, hold a key for 2 seconds.', 'TRIG blinks fast, then all LEDs blink 5 times'],
    ['Back to the knobs', 'Hold the switch, tap the top C.', 'All LEDs blink once'],
    ['Cables only', 'Hold the switch, hold the top C for 2 seconds: the sound stays, the cables plugged in take over.', 'All LEDs blink twice'],
    ['Empty key', 'Nothing changes.', 'EG LED blinks twice'],
    ['Save failed', 'Ksoloti: SD card missing or loose. Push it in and save again.', 'PEAK and TRIG blink 8 times'],
]
story.append(gtable(rows, [30 * mm, 120 * mm, None], ('', 'How', 'You see')))
story.append(Spacer(1, 5))
story.append(side(
    stack(p('<b>Stored:</b> VCOs, portamento, mixer, both filters, EG 1, EG 2, the MG and the cables. '
            '<b>Not stored:</b> delay, arp, master tune and volume, so a recall never changes your tempo or echoes.', 'small'),
          p('<b>A sound\'s cables</b> play until you touch a cable on the panel: then all the cables plugged in count again. '
            'The factory sounds and sounds saved before 1.1 have no cables: they play with the cables plugged in.', 'small'),
          p('<b>After a recall</b> the knobs don\'t jump. A knob moves the value from where the sound left it and catches up by the end of its travel.', 'small')),
    stack(p('<b>Where.</b> Ksoloti: <font name="CondB">MS20MEM.BIN</font> on the SD card (delete it for the factory sounds). '
            'Teensy: its own flash, with a backup on an SD card if one is fitted. '
            'Plugins and the app: one file on your computer for every project. Web edition: in the browser.', 'small'),
          p('With the tap-tempo cable patched, memory is off.', 'small')), FULLW * 0.5))
story.append(PageBreak())

# ---------------- 9. factory sounds + patch panel
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
    ['VCO1+2 CV IN', 'Pitch of both VCOs. KBD CV OUT here = mono mode.'],
    ['VCO2 CV IN', 'Added to VCO 2 pitch.'],
    ['HPF / LPF CUTOFF', 'Replaces EG 2 on that filter\'s EG2/EXT knob.'],
    ['INITIAL GAIN', 'The last note keeps sounding at the source\'s level.'],
    ['VCA CTRL IN', 'The output level (tremolo). Once VCA OUT is patched: the VCA below.'],
    ['VCA IN', 'The patch panel\'s VCA: VCA OUT = VCA IN × VCA CTRL IN. Nothing in CTRL IN: closed.'],
    ['EG1 / KBD TRIG IN', 'Replaces the keyboard gate for EG 1 / EG 2.'],
    ['S&amp;H IN / CLOCK', 'Signal to sample (noise) / when (MG square).'],
    ['ESP SIGNAL IN', 'SWITCH here = tap tempo.'],
], [30 * mm, None], ('Input', 'What a cable does'))
story.append(side(stack(tt, Spacer(1, 3), p('The top C is not a sound: it takes you back to the knobs. Overwrite any key with your own sound.', 'cap')),
                  stack(jacks, Spacer(1, 3),
                        p('Outputs: MG TRI (follows WAVEFORM), MG SQ, EG1, EG1 REV, EG2 REV, S&amp;H, VCA OUT, PINK, WHITE, WHEEL, SWITCH, KBD CV, KBD TRIG. '
                          'Not used yet: EXT SIGNAL IN, SIGNAL OUT, PHONES and the other ESP jacks.', 'small')), 132 * mm))
story.append(PageBreak())

# ---------------- 10. first patches
story += [H1('First patches')]
story.append(p('A cable goes from an output to an input (in these drawings outputs are orange, inputs blue). On the MS-20ic you patch with real cables; '
               'in the window you drag from one jack to another. Start from the knobs (switch + top C) and try these.', 'small'))
PATCHW = FULLW * 0.47
def patch(title, cables, then):
    return stack(p(f'<b>{title}</b>', 'h2'),
                 PanelView(PATCHW, region=(100.0, 30.4, 200.0, 73.5), colorjacks=True, cables=cables),
                 p(then, 'small'))
story.append(side(
    stack(patch('Mono lead with glide: KBD CV OUT → VCO1+2 CV IN', [(31, 25)],
                'One voice, lowest note wins. Turn PORTAMENTO up and play legato. Switch RETRIG on in the window: every key starts the envelopes again.'),
          patch('Random filter steps: S&amp;H OUT → LPF CUTOFF', [(7, 17)],
                'The sample and hold picks up noise on every beat of the MG. LPF EG2/EXT sets how far the filter jumps, MG FREQUENCY how fast. PEAK up for the bubbling sound.')),
    stack(patch('Delayed vibrato: through the VCA', [(1, 12), (10, 14), (16, 0)],
                'MG TRI OUT → VCA IN, EG1 OUT → VCA CTRL IN, VCA OUT → TOTAL. Turn FREQ MOD MG/T.EXT up a little and hold a note: '
                'the vibrato comes in after EG 1\'s DELAY and fades in with its ATTACK.'),
          patch('The note that stays: WHEEL OUT → INITIAL GAIN', [(27, 21)],
                'Push the wheel up: the last note you play keeps sounding at the wheel\'s level after you let go. With the delay up it turns into long tails. '
                'MG TRI OUT instead of WHEEL: it pulses.')),
    PATCHW))
story.append(PageBreak())

# ---------------- 11. editions, USB, setup
story += [H1('Editions, USB and setup')]
story.append(cards([
    ('Ksoloti Core', 'Inside the MS-20ic. Install the Ksoloti Patcher 1.1.0 or newer, update the board\'s firmware, open '
     '<font name="CondB" size="7.4">TwentyEngine_Ksoloti_v51.axp</font> and choose Patch &gt; Upload to SD Card as Startup. Sounds are saved on the SD card.'),
    ('Teensy 4.1', 'Inside the MS-20ic, with the PJRC audio shield. Open <font name="CondB">TwentyEngineTeensy.hex</font> in the Teensy Loader and press the button on the Teensy. '
     'Sounds are saved in its own flash; oscillators at 96 kHz. Buttons and key click: the Teensy guide.'),
    ('AU / VST3 plugins', 'Mac (10.13 or newer, Intel and Apple Silicon) and Windows. <b>Twenty Engine UI</b> has the panel window and works with or without an MS-20ic (p. 12). '
     '<b>Twenty Engine MS-20ic</b> has no window: the iC is the interface (p. 13).'),
    ('App', 'Twenty Engine on its own, with the panel window: Mac, Windows and Linux. No DAW needed, and every take is saved as a WAV (p. 14).'),
    ('Web, free', 'In Chrome or Edge on a computer, nothing to install: the panel, the MS-20ic with its LEDs, any MIDI keyboard (p. 14).'),
], 5, 46 * mm, gap=3 * mm))
story.append(Spacer(1, 2))
story.append(side(
    stack(p('<b>MIDI to your computer</b>', 'h2'),
          p('Connect the board\'s USB port and everything you play on the MS-20ic is also sent to your computer. The port is called "Ksoloti" or "Twenty Engine" (Teensy). '
            'Memory keys are sent as CC 102 / 103, so your DAW doesn\'t play them as notes. On the Teensy, notes from the computer also play the engine.', 'small'),
          p('<b>The plugin and your panel.</b> The MS-20ic only sends where its knobs are when you switch it on with the volume knob. '
            'So the plugins remember the last panel they saw and start from it, with the start-up light show. '
            'Moved knobs while no plugin was open? Switch the volume off and on.', 'small')),
    stack(p('<b>CCs</b>', 'h2'),
          gtable([['11', 'SIGNAL LEVEL: play mode'], ['88', 'LOW CUT: delay time / arp rate'], ['89', 'HIGH CUT: feedback / arp octaves'],
                  ['90', 'CV ADJUST: wow / arp order'], ['91', 'THRESHOLD: delay amount / tone'], ['102 / 103', 'Memory key down / up'],
                  ['104', 'Function switch (a Teensy FUNCTION button)'], ['105', 'ESP knobs on the delay layer'], ['106', 'One tap of tap tempo']],
                 [16 * mm, None], ('CC', 'Function'))), FULLW * 0.5))
story.append(Spacer(1, 2))
story.append(p('<b>Installing.</b> Mac: if macOS says it cannot check the installer for malicious software, open '
               'System Settings &gt; Privacy &amp; Security and click <b>Open Anyway</b>; the same for the app the first time. Windows: if "Windows protected your PC" appears, '
               'click <b>More info</b> &gt; <b>Run anyway</b>. Then restart your music software. '
               '<b>Sample rate:</b> set your DAW (or the app\'s Preferences) to your audio device\'s rate. Bluetooth headphones often run at 44.1 kHz only.', 'small'))
story.append(p('<b>Windows: no KORG driver.</b> Twenty Engine works with Windows\' own MIDI driver; KORG\'s USB-MIDI driver can stop the '
               'LEDs and the sound from working right. If it is installed, remove it: <b>Settings &gt; Apps</b>: uninstall '
               '<b>KORG USB-MIDI Driver</b> (every entry). Then <b>Device Manager &gt; Sound, video and game controllers</b>: right-click '
               '<b>KORG MS-20 Controller</b> &gt; <b>Uninstall device</b>, tick <b>Delete the driver software for this device</b>. '
               'Restart and plug the MS-20ic in again: it now shows as <b>MS-20 Controller</b>.', 'small'))
story.append(p('Wiring and step-by-step installation are in the separate <b>build guide</b>. '
               'Twenty Engine is free software under the GNU GPL v3, built on Axoloti/Ksoloti, PJRC Teensy, iPlug2, DPF and the VST 3 SDK. '
               'MS-20 is a trademark of KORG Inc.; this is an independent project, not affiliated with or endorsed by KORG.', 'cap'))
story.append(PageBreak())

# ---------------- 12. the window
story += [H1('Twenty Engine UI: the panel on screen')]
story.append(p('The whole MS-20 in a window: turn the knobs, flip the switches, patch cables, watch the LEDs. With an MS-20ic connected the window follows the iC; '
               'without one, Twenty Engine UI is a normal synth plugin you play from any keyboard. The app has the same window.', 'lead'))
left = stack(
    p('<b>Knobs.</b> Drag up or right to turn up, down or left to turn down; Shift or Ctrl for fine moves, double-click for the default. Rotary switches step with a click.', 'small'),
    p('<b>Cables.</b> Drag from a jack to another jack. Pick a cable up by its plug to move it, and drop it anywhere else to pull it out. '
      'Point at a jack to see what it does; grey jacks are not used yet.', 'small'),
    p('<b>The status bar</b> shows the mode (click it), what the mouse points at, mono or poly and the play mode, <b>SWITCH</b> and <b>LOG</b>. '
      'SWITCH is the momentary switch: hold it with the mouse and play a key to recall a sound.', 'small'),
    p('<b>Only in the window:</b> <b>RETRIG</b> under PORTAMENTO TIME (p. 6). <b>DELAY / KNOBS</b> next to DELAY AMOUNT puts the ESP knobs on the delay while the arp runs, '
      'as holding the switch, without silencing the keys; the iC\'s ESP knobs follow. <b>SYNC / DAW TEMPO</b>: the arp follows the DAW (p. 6).', 'small'),
    p('<b>Bug log.</b> Something odd? Click <b>LOG</b>: what happened lately is copied, with your setup. Click the blue address and an email to '
      'twentyenginesupport@gmail.com opens: paste the log into it. Twenty Engine MS-20ic has <b>Copy log</b> in its small window.', 'small'))
right = stack(
    p('<b>Two modes</b>', 'h2'),
    gtable([['Automatic', 'MS-20ic mode while an MS-20ic is connected, standard mode otherwise. The default.'],
            ['MS-20ic mode', 'The iC plays it, and in Logic its regions keep their sound (p. 13). Pick it by hand if the iC reaches the plugin only through the DAW (Windows 10).'],
            ['Standard mode', 'A normal synth plugin: the window\'s knobs and cables, automation and presets. Nothing goes into the regions.']],
           [26 * mm, None], ('Mode', '')),
    Spacer(1, 2),
    p('Click the mode at the left of the status bar to change it. Your choice holds for every project.', 'small'),
    p('<b>Presets.</b> The DAW\'s preset menu lists <b>Panel</b> and the 36 sounds: picking one recalls it like switch + key. A preset you save keeps the whole sound, '
      'cables included. Ableton Live on the Mac shows only preset files, so the installer puts the 37 sounds in your Presets folder.', 'small'),
    p('<b>CC learn.</b> Right-click a knob, choose <b>Learn MIDI controller</b> and move a knob or slider on your controller; a blue ring shows the knob waiting. '
      '<b>Forget</b> removes it. One map for your setup: every project uses it, presets don\'t carry it. With an MS-20ic, its own knobs, switches and cables '
      'can\'t be learned: they already have their place.', 'small'))
story.append(side(left, right, FULLW * 0.47))
story.append(Spacer(1, 4))
story.append(picture('panel_window.png', 128 * mm))
story.append(PageBreak())

# ---------------- 13. recording with the plugin
story += [H1('Recording with the plugin')]
story.append(p('In Logic the sound is recorded <b>with every region</b>: cut, move, copy or loop a region and it keeps its own sound, '
               'whatever you do with the knobs afterwards. Your panel stays yours the whole time. This is Twenty Engine MS-20ic, and Twenty Engine UI in MS-20ic mode.', 'lead'))
story.append(side(
    stack(p('<b>Record as usual</b>', 'h2'),
          gtable([['1', 'Put Twenty Engine on an instrument track. Leave its MIDI input on <b>All</b> (Logic sets it so).'],
                  ['2', 'Record. With your notes the region gets the sound: the whole panel when you start, then every knob move, switch, cable and sound recall.'],
                  ['3', 'Play it back: each region plays with its own sound. Play along live on top: the live sound follows the knobs as they are now.'],
                  ['4', 'A new sound for the next take: just dial it in and record. Earlier regions keep theirs.']],
                 [7 * mm, None], ('', 'How')),
          Spacer(1, 3),
          p('A region without a recorded sound plays the panel\'s sound, like any synth. '
            'In standard mode (no MS-20ic) Twenty Engine UI records like any plugin: knob moves in the window are automation.', 'small')),
    stack(p('<b>Good to know</b>', 'h2'),
          p('<b>Start anywhere.</b> Once a part has played, starting or jumping inside it gives its sound at once. '
            'In a part not played since you opened the project, the sound arrives within a second.', 'small'),
          p('<b>Several tracks.</b> The selected (or armed) track plays from the MS-20ic; the others play their regions. '
            'Select another track and play a note: it takes over.', 'small'),
          p('<b>Where the sound lives.</b> As MIDI controllers on channels 15 and 16 in the region. Leave them in: '
            'delete them and the region plays the panel\'s sound. The MS-20ic\'s own knob CCs (channel 1) are in the region too; the plugin ignores them.', 'small')), FULLW * 0.5))
story.append(Spacer(1, 4))
story.append(p('<b>Other DAWs and Windows.</b> Tested in Logic; REAPER should work the same way. In Ableton Live and other DAWs, and on Windows, '
               'the plugin plays like any synth: from the track, with the panel\'s sound. To keep a part\'s sound there, freeze or bounce the track. '
               'Windows: add it to an instrument track with the MS-20ic as its MIDI input, and for the panel LEDs don\'t enable the MS-20ic as a MIDI output in your DAW. '
               'With the free loopMIDI, Windows keeps each take\'s sound too: see the <b>Windows quick guide</b>.', 'small'))
story.append(PageBreak())

# ---------------- 14. app + web
story += [H1('The app and the web edition')]
story.append(side(
    stack(p('<b>Twenty Engine, the app</b>', 'h2'),
          p('Twenty Engine without a DAW, with the panel window. The Mac installer puts <b>Twenty Engine</b> in Applications; on Windows it is in the Start menu. '
            'Linux (beta): unpack it and start <b>TwentyEngine</b>; it plays through JACK, or ALSA / PulseAudio.', 'small'),
          p('<b>Settings.</b> Mac: Twenty Engine &gt; Preferences. Windows: File &gt; Preferences. Pick your audio output there, and a MIDI keyboard if you play from another one. '
            'The MS-20ic is found by itself: just plug it in.', 'small'),
          p('<b>Every take is saved.</b> A take starts with the first note and ends after two seconds of silence. It is saved as a 24-bit WAV in '
            '<b>Music / Twenty Engine / Captures</b>, which keeps the newest 1 GB. The status bar shows it; click there to open the folder.', 'small'),
          p('The app shares the 36 sounds with the plugins, and starts with the sound and window you left. '
            'Key Click has no setting in the app: it stays at 3, the standard.', 'small')),
    stack(p('<b>The web edition (free)</b>', 'h2'),
          p(f'Open <b>{WEB}</b> in Chrome or Edge on a computer and click START. Allow MIDI when the browser asks, and SysEx too: that lights the iC\'s LEDs.', 'small'),
          p('The MS-20ic plays it with its keys, knobs and cables. Any other MIDI keyboard plays the keys, and so does the computer: '
            'A W S E D F T G Y H U J K, with Z and X for the octave.', 'small'),
          p('Sounds you save on the keys and learned controllers stay in that browser. Takes are not saved: that is the app. '
            'Safari and Firefox have no Web MIDI, and phones and tablets are not supported.', 'small'),
          p('<b>Bug log</b> in both: LOG in the status bar, as in the plugin (p. 12).', 'small'),
          Spacer(1, 3), picture('web_page.png', FULLW * 0.5 - 10 * mm)), FULLW * 0.5))

doc = BaseDocTemplate(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TwentyEngine_Manual.pdf'), pagesize=PAGE,
                      leftMargin=M, rightMargin=M, topMargin=14 * mm, bottomMargin=14 * mm,
                      title=f"{NAME} owner's manual", author='Pete')
fr = Frame(M, 14 * mm, PWID - 2 * M, PHGT - 28 * mm, id='f', leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
doc.addPageTemplates([PageTemplate('cover', [fr], onPage=cover), PageTemplate('body', [fr], onPage=body)])
doc.build(story)
print('ok')
