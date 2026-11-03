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
BG = HexColor('#121212'); INK = HexColor(os.environ.get('INK', '#c4c3be')); NEW = HexColor('#e8542a'); DIM = HexColor('#8a8984')
W, H = float(os.environ.get('W', 45)) * mm, float(os.environ.get('H', 12)) * mm   # PLACEHOLDER size until measured
LW = 0.32 * mm                                   # Barlow Semi Condensed regular stem weight at the ESP label's text size

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

GLYPH_W = {'tri': 1.35, 'saw': 1.15, 'pulse': 1.6, 'noise': 2.4}   # width / height, as on the panel's VCO1 switch
def glyph(c, x, y, w, h, kind):
    """the VCO1 waveform-switch symbols in the panel's proportions, centred in the box"""
    gh = h * 0.8; gw = min(w, gh * GLYPH_W[kind])
    if gw < gh * GLYPH_W[kind]: gh = gw / GLYPH_W[kind]               # keep the proportion if the box is narrow
    x0 = x + (w - gw) / 2; y0 = y + (h - gh) / 2
    P = lambda u, v: (x0 + gw * u, y0 + gh * v)
    c.setStrokeColor(INK); c.setLineWidth(LW); c.setLineJoin(0); c.setLineCap(0)
    if kind == 'tri':     path(c, [P(0, 0), P(0.5, 1), P(1, 0)])
    elif kind == 'saw':   path(c, [P(0, 0), P(0, 1), P(1, 0)])
    elif kind == 'pulse': path(c, [P(0, 0), P(0.12, 0), P(0.12, 1), P(0.5, 1), P(0.5, 0), P(1, 0)])
    elif kind == 'noise':                                             # wide, low, irregular, a few taller spikes
        v = [0.45, 0.62, 0.38, 0.75, 0.30, 0.95, 0.20, 0.66, 0.35, 0.80, 0.28, 0.58, 0.45]
        path(c, [P(i / (len(v) - 1), v[i]) for i in range(len(v))])

def arrow(c, x0, x1, y):
    c.setStrokeColor(DIM); c.setLineWidth(LW); c.setLineCap(2); c.setLineJoin(0); c.line(x0, y, x1, y)
    a = 0.7 * mm; path(c, [(x1 - a, y + a * 0.6), (x1, y), (x1 - a, y - a * 0.6)])

def chevron(c, x, y, h):
    c.setStrokeColor(DIM); c.setLineWidth(LW); c.setLineCap(2); c.setLineJoin(0)
    path(c, [(x, y + h / 2), (x + h * 0.6, y), (x, y - h / 2)])

def mix(c0, c1, t):
    return HexColor('#%02x%02x%02x' % tuple(int(round(c0[i] + (c1[i] - c0[i]) * t)) for i in range(3)))
WHITE_RGB = (0xc4, 0xc3, 0xbe); ORANGE_RGB = (0xe8, 0x54, 0x2a)   # the panel's light grey, Twenty Engine orange

def morph(c, x, y, w, h, kind, cycles=3, N=420):
    """one continuous wave that morphs from knob-left to knob-right, colour fading towards the new (orange) sound"""
    m = y + h / 2; A = h / 2; pts = []
    if kind == 'pulse':                         # exact vertical edges, the pulse narrows cycle by cycle
        for k in range(cycles):
            t0 = k / cycles; t1 = (k + 1) / cycles; d = 0.5 - 0.36 * (k + 0.5) / cycles
            xe = t0 + (t1 - t0) * d
            pts += [(x + w * t0, m - A, t0), (x + w * t0, m + A, t0), (x + w * xe, m + A, xe), (x + w * xe, m - A, xe)]
        pts.append((x + w, m - A, 1.0)); N = len(pts) - 1
    elif kind == 'noise':                       # white -> pink: random steps that get slower and smoother along the way
        rnd = random.Random(21); t = 0.0; pts.append((x, m + A * rnd.uniform(-1, 1), 0.0)); prev = 0.0
        while t < 1.0:
            t = min(1.0, t + 0.011 + 0.05 * t * t)          # steps get longer: slower, lower-pitched movement (pink)
            r = rnd.uniform(-1, 1); a = 0.55 * t; prev = a * prev + (1 - a) * r     # and a little smoother
            v = prev / max(0.45, 1 - 0.55 * a)                                    # keep the level up
            pts.append((x + w * t, m + A * max(-1, min(1, v)), t))
        N = len(pts) - 1
    else:
        for i in range(N + 1):
            t = i / N; ph = (cycles * t) % 1.0
            if kind == 'tri':                       # sine -> triangle
                tri = 4 * ph - 1 if ph < 0.5 else 3 - 4 * ph
                v = (1 - t) * math.sin(2 * math.pi * (ph - 0.25)) + t * tri
            else:                                   # rising saw -> saw with phaser notches
                v = (2 * ph - 1) * (1 - 0.25 * t) + 0.25 * t * math.sin(2 * math.pi * 4 * ph)
            pts.append((x + w * t, m + A * v, t))
    c.setLineWidth(LW); c.setLineCap(1); c.setLineJoin(1)
    step = 1 if kind in ('noise', 'pulse') else 2
    for i in range(0, N, step):                 # short segments so the colour can fade
        x0, y0, t0 = pts[i]; x1, y1, _ = pts[min(N, i + step)]
        if kind == 'tri': col = mix(ORANGE_RGB, WHITE_RGB, t0)           # the new part (sine) is at the start
        elif kind == 'pulse': col = mix(WHITE_RGB, WHITE_RGB, 0)          # all original MS-20
        else: col = mix(WHITE_RGB, ORANGE_RGB, t0)                        # the new part is at the end
        c.setStrokeColor(col); c.line(x0, y0, x1, y1)

def cell(c, x, y, sh, g, kind, ww):
    global INK
    gw = 3.6 * mm
    keep = INK; INK = DIM; glyph(c, x, y, gw, sh, g); INK = keep      # switch symbol, dim
    p = 0.7 * mm                                                        # thin frame: "this is a switch position you select"
    c.setStrokeColor(DIM); c.setLineWidth(0.22 * mm); c.setLineJoin(0)
    c.rect(x - p, y - p, gw + 2 * p, sh + 2 * p, stroke=1, fill=0)
    morph(c, x + gw + 2.3 * mm, y, ww, sh, kind)
    return gw + 2.3 * mm + ww

def sticker(c, ox, oy):
    c.setFillColor(BG); c.rect(ox, oy, W, H, stroke=0, fill=1)
    grid = [[('tri', 'tri'), ('saw', 'saw')], [('pulse', 'pulse'), ('noise', 'noise')]]   # reading order = waveform switch order
    sh = 2.6 * mm; ww = 11.5 * mm; cw = 3.6 * mm + 2.3 * mm + ww; colgap = 4.0 * mm; rowgap = 3.0 * mm
    x0 = ox + (W - (2 * cw + colgap)) / 2; y0 = oy + (H - (2 * sh + rowgap)) / 2
    for r, row in enumerate(grid):
        for k, (g, kind) in enumerate(row):
            cell(c, x0 + k * (cw + colgap), y0 + (1 - r) * (sh + rowgap), sh, g, kind, ww)

def crop(c, ox, oy, L=3 * mm, g=1.2 * mm):
    c.setStrokeColor(HexColor('#000000')); c.setLineWidth(0.25)
    for x in (ox, ox + W):
        c.line(x, oy - g, x, oy - g - L); c.line(x, oy + H + g, x, oy + H + g + L)
    for y in (oy, oy + H):
        c.line(ox - g, y, ox - g - L, y); c.line(ox + W + g, y, ox + W + g + L, y)

if __name__ == '__main__':
    c = canvas.Canvas(sys.argv[1], pagesize=A4)
    ox, oy = 40 * mm, 220 * mm; sticker(c, ox, oy); crop(c, ox, oy)
    c.setFont('Helvetica', 8); c.drawString(20 * mm, 272 * mm, 'Twenty Engine - PW knob legend (print at 100 %%). DRAFT size: %.0f x %.0f mm' % (W / mm, H / mm))
    c.showPage(); c.save()
