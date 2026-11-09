# The ESP knob label (see label/label.py) redrawn in the manual's colours:
# white sticker, black line art, arp items in orange italic, knobs drawn in the holes.
# Geometry follows label.py (130 x 33.5 mm, 104 mm knob span, 20 mm skirts).
import os, math
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

LBL = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'label')
if not os.path.isdir(LBL): LBL = '/home/claude/twenty-engine/label'
pdfmetrics.registerFont(TTFont('Barlow', os.path.join(LBL, 'BarlowSC-400.ttf')))
pdfmetrics.registerFont(TTFont('BarlowI', os.path.join(LBL, 'BarlowSC-400i.ttf')))

INK = HexColor('#151515'); ARPC = HexColor('#d9481f'); CALL = HexColor('#e8542a'); RULE = HexColor('#c8c8c8')
W, H, SPAN, SKIRT, ROW_Y = 130.0, 33.5, 104.0, 20.0, 15.0
NAME_PT = 7.8; SLANT = 10.0


def _ital(c, x, y, t, size=NAME_PT):
    c.saveState(); c.translate(x, y); c.skew(0, SLANT)
    c.setFillColor(ARPC); c.setFont('BarlowI', size); c.drawCentredString(0, 0, t); c.restoreState()


def _plain(c, x, y, t, size=NAME_PT):
    c.setFillColor(INK); c.setFont('Barlow', size); c.drawCentredString(x, y, t)


def _tick(c, xc, cy, r0, f, L, w=0.36 * mm):
    a = math.radians(225.0 - 270.0 * f)
    c.setStrokeColor(INK); c.setLineWidth(w); c.setLineCap(0)
    c.line(xc + math.cos(a) * r0, cy + math.sin(a) * r0, xc + math.cos(a) * (r0 + L), cy + math.sin(a) * (r0 + L))


def _knob(c, x, y, r, f):
    c.setFillColor(white); c.setStrokeColor(INK); c.setLineWidth(0.5)
    c.circle(x, y, r, stroke=1, fill=1)
    a = math.radians(225.0 - 270.0 * f)
    c.setLineWidth(r * 0.14); c.setLineCap(1)
    c.line(x + r * 0.12 * math.cos(a), y + r * 0.12 * math.sin(a), x + r * 0.86 * math.cos(a), y + r * 0.86 * math.sin(a))
    c.setLineCap(0)


def knob_x(i):
    """centre of knob i (0..4) in mm from the label's left edge"""
    return (W - SPAN) / 2 + i * SPAN / 4


def draw(c, values=(0.42, 0.5, 0.45, 0.3, 0.35), callouts=(32, 33, 34, 35, 36), frame=True):
    """draws the label at 1:1 (in mm) with its lower left corner at the origin"""
    R = SKIRT / 2 * mm; cy = ROW_Y * mm
    if frame:
        c.setFillColor(white); c.setStrokeColor(INK); c.setLineWidth(0.8)
        c.rect(0, 0, W * mm, H * mm, stroke=1, fill=1)
    xs = [knob_x(i) * mm for i in range(5)]
    ydly = cy + R + 2.6 * mm; ytop = ydly + 3.0 * mm; ybot = 2.4 * mm
    arp = [None, 'RATE', 'OCTAVES', 'ORDER', None]
    dly = [None, 'TIME', 'FEEDBACK', 'WOW / FLUTTER', 'DELAY AMOUNT']
    for i, x in enumerate(xs):
        if arp[i]: _ital(c, x, ytop, arp[i])
        if dly[i]: _plain(c, x, ydly, dly[i])
    # play mode arcs: poly (thin, black), arp and latch (thick, orange)
    x = xs[0]; ra = R + 0.9 * mm
    ang = lambda f: 225.0 - 270.0 * f
    for f0, f1, isarp in ((0.0, 18 / 64, False), (18 / 64, 42 / 64, True), (42 / 64, 1.0, True)):
        a0 = ang(f0) - 2.5; a1 = ang(f1) + 2.5
        rr = ra if isarp else ra - 0.35 * mm
        c.setStrokeColor(ARPC if isarp else INK); c.setLineCap(0); c.setLineWidth(0.8 * mm if isarp else 0.36 * mm)
        p = c.beginPath(); p.arc(x - rr, cy - rr, x + rr, cy + rr, startAng=a1, extent=a0 - a1); c.drawPath(p, stroke=1, fill=0)
    poly_x = x + math.cos(math.radians(236.0)) * (ra + 3.2 * mm)
    latch_x = x + math.cos(math.radians(-56.0)) * (ra + 3.6 * mm)
    _plain(c, poly_x, ybot + 0.2 * mm, 'POLY')
    _ital(c, x, ydly, 'VCO2 ARP')
    _ital(c, latch_x, ybot + 0.2 * mm, 'LATCH')
    # growing ticks on time, feedback, wow and delay amount
    for xi in (1, 2, 3, 4):
        for k in range(11):
            _tick(c, xs[xi], cy, R + 0.6 * mm, k / 10.0, (0.45 + 0.17 * k) * mm)
    # octave numbers and order symbols in four zones
    def zone_xy(xc, k, r):
        d = 225.0 - 270.0 * (k + 0.5) / 4.0
        d += 21.0 if k == 0 else (-21.0 if k == 3 else 0.0)
        a = math.radians(d); return xc + math.cos(a) * r, cy + math.sin(a) * r
    rz = R + 3.3 * mm
    for k, t in enumerate('1234'):
        px, py = zone_xy(xs[2], k, rz)
        c.saveState(); c.translate(px, py); c.skew(0, SLANT)
        c.setFillColor(ARPC); c.setFont('BarlowI', 7.2); c.drawCentredString(0, -0.9 * mm, t); c.restoreState()
    pats = [[0.45, 0.85, 1.25, 1.65], [1.65, 1.25, 0.85, 0.45], [0.5, 1.1, 1.65, 1.1, 0.5], [1.15, 0.45, 1.65, 0.8]]
    bw, bg = 0.32 * mm, 0.24 * mm
    for k, hs in enumerate(pats):
        px, py = zone_xy(xs[3], k, rz + 0.4 * mm)
        tw = len(hs) * bw + (len(hs) - 1) * bg
        c.saveState(); c.translate(px, py - 0.8 * mm); c.skew(0, SLANT + 10.0); c.setFillColor(ARPC)
        for j, h in enumerate(hs): c.rect(-tw / 2 + j * (bw + bg), 0, bw, h * mm, stroke=0, fill=1)
        c.restoreState()
    # the knobs themselves, in the holes
    for x, v in zip(xs, values): _knob(c, x, cy, R * 0.78, v)
    # number callouts (manual style), upper left of each knob
    if callouts:
        for x, n in zip(xs, callouts):
            px, py = x, cy - R - 0.2 * mm
            c.setFillColor(CALL); c.circle(px, py, 1.55 * mm, stroke=0, fill=1)
            c.setFillColor(white); c.setFont('Helvetica-Bold', 5.6); c.drawCentredString(px, py - 2.0, str(n))
