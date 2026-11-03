# PW knob legend for the MS-20 iC (Twenty Engine): what PW does for each VCO1 waveform, as symbols.
# Rows in the order of the VCO1 waveform switch. Each row: knob fully left -> knob fully right.
#   triangle: sine -> triangle | saw: clean saw -> saw + phaser | pulse: square -> narrow pulse | noise: white -> pink
import os, sys, math, random
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
HERE = os.path.dirname(os.path.abspath(__file__))
pdfmetrics.registerFont(TTFont('Cond', HERE + '/BarlowSC-400.ttf'))
BG = HexColor('#121212'); INK = HexColor(os.environ.get('INK', '#ecebe6')); NEW = HexColor('#e8542a'); DIM = HexColor('#8a8984')
W, H = float(os.environ.get('W', 42)) * mm, float(os.environ.get('H', 12)) * mm   # PLACEHOLDER size until measured
LW = 0.26 * mm                                   # Barlow Semi Condensed regular stem weight at the ESP label's text size

def path(c, pts):
    p = c.beginPath(); p.moveTo(*pts[0])
    for q in pts[1:]: p.lineTo(*q)
    c.drawPath(p, stroke=1, fill=0)

def wave(c, x, y, w, h, kind):
    """draw one symbol in the box (x, y) - (x+w, y+h), centred vertically on y + h/2"""
    m = y + h / 2; A = h / 2
    c.setStrokeColor(INK); c.setLineWidth(LW); c.setLineJoin(0); c.setLineCap(2)   # square ends + sharp joins, like Barlow
    N = 60
    if kind == 'sine':
        path(c, [(x + w * i / N, m + A * math.sin(2 * math.pi * i / N)) for i in range(N + 1)])
    elif kind == 'tri':
        pts = []; 
        for i in range(N + 1):
            ph = (1.0 * i / N + 0.25) % 1.0; v = 4 * ph - 1 if ph < 0.5 else 3 - 4 * ph
            pts.append((x + w * i / N, m + A * v))
        path(c, pts)
    elif kind in ('saw', 'phaser'):
        cyc = 1; pts = []
        for k in range(cyc):
            x0 = x + w * k / cyc; x1 = x + w * (k + 1) / cyc
            if kind == 'saw':
                pts += [(x0, m - A), (x1, m + A)]
            else:                                # phased saw: the ramp gets notches (comb) along the way
                for j in range(13):
                    t = j / 12.0; ripple = 0.28 * A * math.sin(2 * math.pi * 3 * t) * (1 if j % 12 else 0)
                    pts.append((x0 + (x1 - x0) * t, m - A + 2 * A * t + ripple))
            pts.append((x1, m - A))
        path(c, [(px, 2 * m - py) for px, py in pts])     # falling ramp, like the panel's saw symbol
    elif kind in ('square', 'narrow'):
        duty = 0.5 if kind == 'square' else 0.18; cyc = 1; pts = [(x, m - A)]
        for k in range(cyc):
            x0 = x + w * k / cyc; xm = x0 + w / cyc * duty; x1 = x + w * (k + 1) / cyc
            pts += [(x0, m + A), (xm, m + A), (xm, m - A), (x1, m - A)]
        path(c, pts)
    elif kind in ('white', 'pink'):
        rnd = random.Random(7 if kind == 'white' else 11); n = 14 if kind == 'white' else 14; pts = []; s = 0.0
        for i in range(n + 1):
            r = rnd.uniform(-1, 1)
            if kind == 'white': v = r
            else: s = 0.72 * s + 0.6 * r; v = max(-1, min(1, s))     # smoother, bigger swings (low end weighted)
            pts.append((x + w * i / n, m + A * 0.95 * v))
        path(c, pts)

def glyph(c, x, y, w, h, kind):
    m = y + h / 2; A = h / 2
    c.setStrokeColor(INK); c.setLineWidth(LW); c.setLineJoin(0); c.setLineCap(2)
    if kind == 'tri':   path(c, [(x, m - A), (x + w / 2, m + A), (x + w, m - A)])
    elif kind == 'saw': path(c, [(x, m - A), (x, m + A), (x + w, m - A)])
    elif kind == 'pulse': path(c, [(x, m - A), (x + w * 0.2, m - A), (x + w * 0.2, m + A), (x + w * 0.6, m + A), (x + w * 0.6, m - A), (x + w, m - A)])
    elif kind == 'noise':
        rnd = random.Random(3); path(c, [(x + w * i / 9, m + A * rnd.uniform(-1, 1)) for i in range(10)])

def arrow(c, x0, x1, y):
    c.setStrokeColor(DIM); c.setLineWidth(LW); c.setLineCap(2); c.setLineJoin(0); c.line(x0, y, x1, y)
    a = 0.7 * mm; path(c, [(x1 - a, y + a * 0.6), (x1, y), (x1 - a, y - a * 0.6)])

def chevron(c, x, y, h):
    c.setStrokeColor(DIM); c.setLineWidth(LW); c.setLineCap(2); c.setLineJoin(0)
    path(c, [(x, y + h / 2), (x + h * 0.6, y), (x, y - h / 2)])

def cell(c, x, y, sh, g, a, b):
    """one waveform: dim switch symbol, then knob left > knob right"""
    global INK
    gw = 2.4 * mm; sw = 4.6 * mm; ym = y + sh / 2
    keep = INK; INK = DIM; glyph(c, x, y, gw, sh, g); INK = keep
    x += gw + 2.0 * mm
    for kind, xx in ((a, x), (b, x + sw + 3.8 * mm)):
        keep = INK
        if kind in ('sine', 'phaser', 'pink'): INK = NEW          # Twenty Engine additions, not on the original MS-20
        wave(c, xx, y, sw, sh, kind); INK = keep
    chevron(c, x + sw + 1.5 * mm, ym, sh * 0.6)
    return gw + 2.0 * mm + 2 * sw + 3.8 * mm                       # cell width

def sticker(c, ox, oy):
    c.setFillColor(BG); c.rect(ox, oy, W, H, stroke=0, fill=1)
    grid = [[('tri', 'sine', 'tri'), ('saw', 'saw', 'phaser')],     # reading order = waveform switch order
            [('pulse', 'square', 'narrow'), ('noise', 'white', 'pink')]]
    sh = 2.6 * mm; cw = 2.4 * mm + 2.0 * mm + 2 * 4.6 * mm + 3.8 * mm; colgap = 4.0 * mm; rowgap = 3.0 * mm
    x0 = ox + (W - (2 * cw + colgap)) / 2; y0 = oy + (H - (2 * sh + rowgap)) / 2
    for r, row in enumerate(grid):
        for k, (g, a, b) in enumerate(row):
            cell(c, x0 + k * (cw + colgap), y0 + (1 - r) * (sh + rowgap), sh, g, a, b)

def crop(c, ox, oy, L=3 * mm, g=1.2 * mm):
    c.setStrokeColor(HexColor('#000000')); c.setLineWidth(0.25)
    for x in (ox, ox + W):
        c.line(x, oy - g, x, oy - g - L); c.line(x, oy + H + g, x, oy + H + g + L)
    for y in (oy, oy + H):
        c.line(ox - g, y, ox - g - L, y); c.line(ox + W + g, y, ox + W + g + L, y)

c = canvas.Canvas(sys.argv[1], pagesize=A4)
ox, oy = 40 * mm, 220 * mm; sticker(c, ox, oy); crop(c, ox, oy)
c.setFont('Helvetica', 8); c.drawString(20 * mm, 272 * mm, 'Twenty Engine - PW knob legend (print at 100 %%). DRAFT size: %.0f x %.0f mm' % (W / mm, H / mm))
c.showPage(); c.save()
