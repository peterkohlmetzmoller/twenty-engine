# Original line-art drawings of the MS-20 iC panel as this engine uses it.
# All coordinates are local units (1 unit = 1 mm at scale 1), y up, origin bottom-left of the panel.
import math
from reportlab.lib.colors import HexColor, white, black

PANEL = white
EDGE = black
INK = HexColor('#111111')        # line art
GREY = HexColor('#8c8c8c')       # patch panel print (lighter, as on patch sheets)
DIM = HexColor('#b8b8b8')
ACCENT = HexColor('#e8542a')     # callouts
OUTC = HexColor('#f0901f')       # active output jacks
INC = HexColor('#2f9fd0')        # active input jacks
MONOC = HexColor('#5fb53a')      # mono-mode gesture jack
LEDON = HexColor('#e8321f')

FONT = 'Cond'
FONTB = 'CondB'

PW, PH = 200.0, 86.0             # whole panel
LW = 100.0                        # left (knob) area width

# ---------------------------------------------------------------- symbols
def wave(c, kind, x, y, s, col=INK, lw=0.28):
    c.saveState(); c.setStrokeColor(col); c.setLineWidth(lw); c.setLineJoin(1)
    h = s * 0.5; w = s
    p = c.beginPath()
    if kind == 'tri':
        p.moveTo(x - w/2, y - h/2); p.lineTo(x - w/4, y + h/2); p.lineTo(x, y - h/2); p.lineTo(x + w/4, y + h/2); p.lineTo(x + w/2, y - h/2)
    elif kind == 'saw':
        p.moveTo(x - w/2, y - h/2); p.lineTo(x, y + h/2); p.lineTo(x, y - h/2); p.lineTo(x + w/2, y + h/2); p.lineTo(x + w/2, y - h/2)
    elif kind in ('sq', 'pulse', 'npulse'):
        d = {'sq': 0.5, 'pulse': 0.35, 'npulse': 0.15}[kind]
        x0 = x - w/2
        p.moveTo(x0, y - h/2); p.lineTo(x0, y + h/2); p.lineTo(x0 + w/2*d*2, y + h/2); p.lineTo(x0 + w/2*d*2, y - h/2)
        p.lineTo(x0 + w/2, y - h/2); p.lineTo(x0 + w/2, y + h/2); p.lineTo(x0 + w/2 + w/2*d*2, y + h/2); p.lineTo(x0 + w/2 + w/2*d*2, y - h/2); p.lineTo(x + w/2, y - h/2)
    elif kind == 'noise':
        import random
        r = random.Random(3); p.moveTo(x - w/2, y)
        for i in range(1, 13):
            p.lineTo(x - w/2 + w*i/12, y + (r.random()-0.5)*h*1.2)
    elif kind == 'sine':
        p.moveTo(x - w/2, y)
        for i in range(1, 25):
            t = i/24; p.lineTo(x - w/2 + w*t, y + math.sin(t*2*math.pi)*h/2)
    elif kind == 'ring':
        c.circle(x, y, h*0.55, stroke=1, fill=0)
        p.moveTo(x - h*0.39, y - h*0.39); p.lineTo(x + h*0.39, y + h*0.39)
        p.moveTo(x - h*0.39, y + h*0.39); p.lineTo(x + h*0.39, y - h*0.39)
    elif kind == 'rnd':
        pts = [0, .7, .2, .9, -.6, -.2, .5, -.8, .1]
        p.moveTo(x - w/2, y + pts[0]*h/2)
        for i, v in enumerate(pts[1:], 1):
            p.lineTo(x - w/2 + w*i/8, y + v*h/2)
    c.drawPath(p, stroke=1, fill=0); c.restoreState()


def text(c, x, y, s, size=2.2, font=FONT, col=INK, anchor='c'):
    c.setFillColor(col); c.setFont(font, size)
    lines = s.split('\n')
    for i, ln in enumerate(lines):
        yy = y - i * size * 1.08
        if anchor == 'c': c.drawCentredString(x, yy, ln)
        elif anchor == 'l': c.drawString(x, yy, ln)
        else: c.drawRightString(x, yy, ln)


def callout(c, x, y, n, r=1.55):
    c.setFillColor(ACCENT); c.setStrokeColor(white); c.setLineWidth(0.25)
    c.circle(x, y, r, stroke=1, fill=1)
    c.setFillColor(white); c.setFont(FONTB, r * 1.25 if n < 10 else r * 1.05)
    c.drawCentredString(x, y - r*0.42, str(n))


def led(c, x, y, on=True, label=None, r=0.7):
    c.setFillColor(LEDON if on else white); c.setStrokeColor(INK); c.setLineWidth(0.25)
    c.circle(x, y, r, stroke=1, fill=1)
    if label: text(c, x, y + r + 0.9, label, 1.7)


def knob(c, x, y, r, v, label=None, bip=False, numbers=False, sub=None, rays=True):
    c.setStrokeColor(INK); c.setLineCap(0)
    if rays:
        c.setLineWidth(0.3)
        for i in range(11):
            a = math.radians(240 - 30 * i)
            r1, r2 = r * 1.12, r * 1.5
            c.line(x + r1*math.cos(a), y + r1*math.sin(a), x + r2*math.cos(a), y + r2*math.sin(a))
            if numbers:
                lab = str(i - 5) if bip else str(i)
                rr = r * 1.85
                c.setFillColor(INK); c.setFont(FONT, r * 0.42)
                c.drawCentredString(x + rr*math.cos(a), y + rr*math.sin(a) - r*0.15, lab)
    c.setFillColor(white); c.setLineWidth(0.38)
    c.circle(x, y, r, stroke=1, fill=1)
    if v is not None:
        a = math.radians(240 - 300 * max(0, min(1, v)))
        c.setLineWidth(r * 0.13); c.setLineCap(1)
        c.line(x + r*0.1*math.cos(a), y + r*0.1*math.sin(a), x + r*0.86*math.cos(a), y + r*0.86*math.sin(a))
        c.setLineCap(0)
    if label:
        text(c, x, y - r * 1.62 - 1.5, label, 1.72)


def rotary(c, x, y, r, sel, marks, label=None, numbers=True):
    n = len(marks)
    angs = [140 - i * (100 / (n - 1)) for i in range(n)]
    c.setStrokeColor(INK); c.setLineWidth(0.25)
    for i, a in enumerate(angs):
        ar = math.radians(a)
        c.line(x + r*1.1*math.cos(ar), y + r*1.1*math.sin(ar), x + r*1.3*math.cos(ar), y + r*1.3*math.sin(ar))
        rr = r * 1.75
        mx, my = x + rr*math.cos(ar), y + rr*math.sin(ar)
        m = marks[i]
        if isinstance(m, tuple): wave(c, m[1], mx, my, r * 0.52, lw=0.22)
        else: text(c, mx, my - r*0.18, m, r * 0.48)
    c.setFillColor(white); c.setLineWidth(0.55)
    c.circle(x, y, r, stroke=1, fill=1)
    a = math.radians(angs[sel])
    c.setLineWidth(r * 0.13); c.setLineCap(1)
    c.line(x + r*0.1*math.cos(a), y + r*0.1*math.sin(a), x + r*0.86*math.cos(a), y + r*0.86*math.sin(a))
    c.setLineCap(0)
    if label: text(c, x, y - r * 1.25 - 1.7, label, 1.72)


def jack(c, x, y, label=None, state=None, r=1.55, above=True, num=None):
    col = {'out': OUTC, 'in': INC, 'mono': MONOC}.get(state)
    c.setFillColor(white)
    if col:
        c.setStrokeColor(col); c.setLineWidth(0.7); c.circle(x, y, r * 1.3, stroke=1, fill=1)
        c.setStrokeColor(INK); c.setLineWidth(0.3); c.circle(x, y, r * 0.95, stroke=1, fill=0)
    else:
        c.setStrokeColor(DIM if state == 'off' else INK); c.setLineWidth(0.35); c.circle(x, y, r * 1.2, stroke=1, fill=1)
    if label:
        tc = INK if col else (DIM if state == 'off' else GREY)
        if above: text(c, x, y + r * 1.3 + 0.8 + 1.75 * label.count('\n'), label, 1.75, col=tc)
        else: text(c, x, y - r * 1.3 - 2.2, label, 1.75, col=tc)
    if num is not None:
        c.setFillColor(GREY); c.setFont(FONT, 1.45); c.drawString(x + r*1.3 + 0.25, y - r*1.3, str(num))


def box(c, x0, y0, x1, y1, label, size=1.9, col=None):
    col = col or GREY
    c.setStrokeColor(col); c.setLineWidth(0.25); c.rect(x0, y0, x1 - x0, y1 - y0, stroke=1, fill=0)
    n = label.count('\n') + 1
    text(c, (x0 + x1) / 2, (y0 + y1) / 2 + (n - 1) * size * 0.54 - size * 0.35, label, size, col=col)


def arrow(c, pts, col=None, lw=0.25, head=True):
    col = col or GREY
    c.setStrokeColor(col); c.setLineWidth(lw)
    p = c.beginPath(); p.moveTo(*pts[0])
    for q in pts[1:]: p.lineTo(*q)
    c.drawPath(p, stroke=1, fill=0)
    if head:
        (xa, ya), (xb, yb) = pts[-2], pts[-1]
        a = math.atan2(yb - ya, xb - xa); L = 0.9
        c.setFillColor(col)
        p = c.beginPath(); p.moveTo(xb, yb)
        p.lineTo(xb - L*math.cos(a - 0.45), yb - L*math.sin(a - 0.45)); p.lineTo(xb - L*math.cos(a + 0.45), yb - L*math.sin(a + 0.45)); p.close()
        c.drawPath(p, stroke=0, fill=1)


# ---------------------------------------------------------------- layout: knob area
COLX = [7.5, 20.0, 32.6, 45.0, 57.4, 69.8, 82.2, 94.0]
def Y(yd): return PH - yd
KR = 3.0
RA, RB, RC, RD = 18.0, 35.0, 51.0, 71.0      # VCO rows + portamento/tune
MA, MB = 58.5, 71.0                            # modulation rows (mixer, filters, MG, EG1)
EGR = [17.0, 31.0, 45.0, 58.5, 71.0]

# (n, kind, col, ydown, label, value, extra)
LEFT = [
    (1, 'rot', 0, RA, 'WAVEFORM', 0, [('w', 'tri'), ('w', 'saw'), ('w', 'pulse'), ('w', 'noise')]),
    (2, 'knob', 0, RB, 'PULSE\nWIDTH', 0.0, None),
    (3, 'rot', 0, RC, 'SCALE', 1, ["32'", "16'", "8'", "4'"]),
    (4, 'knob', 0, RD, 'TIME', 0.03, None),
    (5, 'rot', 1, RA, 'WAVEFORM', 0, [('w', 'saw'), ('w', 'sq'), ('w', 'npulse'), ('w', 'ring')]),
    (6, 'knobb', 1, RB, 'PITCH', 0.5, None),
    (7, 'rot', 1, RC, 'SCALE', 1, ["16'", "8'", "4'", "2'"]),
    (8, 'knobb', 1, RD, '', 0.5, None),
    (9, 'knob', 2, RA, 'VCO 1\nLEVEL', 1.0, None),
    (10, 'knob', 2, RB, 'VCO 2\nLEVEL', 0.0, None),
    (11, 'knob', 2, MA, 'MG/T.EXT', 0.0, None),
    (12, 'knob', 2, MB, 'EG1/EXT', 0.0, None),
    (13, 'knob', 3, RA, 'CUTOFF\nFREQUENCY', 0.0, None),
    (14, 'knob', 3, RB, 'PEAK', 0.0, None),
    (15, 'knob', 3, MA, 'MG/T.EXT', 0.0, None),
    (16, 'knob', 3, MB, 'EG2/EXT', 0.68, None),
    (17, 'knob', 4, RA, 'CUTOFF\nFREQUENCY', 0.73, None),
    (18, 'knob', 4, RB, 'PEAK', 0.0, None),
    (19, 'knob', 4, MA, 'MG/T.EXT', 0.0, None),
    (20, 'knob', 4, MB, 'EG2/EXT', 0.0, None),
    (21, 'knob', 5, MA, 'WAVEFORM', 0.0, None),
    (22, 'knob', 5, MB, 'FREQUENCY', 0.02, None),
    (23, 'knob', 6, 46.0, 'DELAY', 0.0, None),
    (24, 'knob', 6, MA, 'ATTACK', 0.31, None),
    (25, 'knob', 6, MB, 'RELEASE', 0.56, None),
    (26, 'knob', 7, EGR[0], 'HOLD', 0.0, None),
    (27, 'knob', 7, EGR[1], 'ATTACK', 0.0, None),
    (28, 'knob', 7, EGR[2], 'DECAY', 0.0, None),
    (29, 'knob', 7, EGR[3], 'SUSTAIN', 1.0, None),
    (30, 'knob', 7, EGR[4], 'RELEASE', 0.02, None),
]
LEFT_POS = {e[0]: (COLX[e[2]], Y(e[3])) for e in LEFT}

# ---------------------------------------------------------------- layout: patch panel (photo-grid -> local units)
RSX = (PW - LW - 2.0) / 1300.0
RSY = PH / 1060.0
def RX(px): return LW + 1.0 + (px - 190) * RSX
def RY(py): return (1140 - py) * RSY

# (jack no, px, py, label, role)
JACKS = [
    (0, 235, 310, 'TOTAL', 'in'), (5, 410, 310, 'FREQ', 'in'), (9, 560, 310, 'EXT\nSIGNAL IN', None),
    (13, 700, 310, 'HPF\nCUTOFF', 'in'), (17, 850, 310, 'LPF\nCUTOFF', 'in'), (21, 1005, 310, 'INITIAL\nGAIN', None),
    (29, 1405, 185, 'SIGNAL OUT', None), (30, 1405, 315, 'PHONES', None),
    (1, 235, 465, 'MG TRI\nOUT', 'out'), (6, 490, 465, 'MG SQ\nOUT', 'out'), (10, 610, 465, 'EG1\nOUT', 'out'),
    (22, 1070, 465, 'EG2\nREV OUT', 'out'), (24, 1160, 555, 'VCO2\nCV IN', 'in'), (25, 1280, 480, 'VCO1+2\nCV IN', 'mono'),
    (31, 1400, 480, 'KBD\nCV OUT', 'out'),
    (4, 365, 590, 'S&H\nCLOCK', 'in'), (11, 610, 590, 'EG1\nREV OUT', 'out'), (14, 725, 590, 'VCA\nCTRL IN', 'in'),
    (18, 915, 590, 'EG1\nTRIG IN', 'in'), (26, 1280, 600, 'KBD\nTRIG IN', 'in'), (32, 1400, 600, 'KBD\nTRIG OUT', 'out'),
    (2, 245, 710, 'S&H IN', 'in'), (7, 490, 710, 'S&H OUT', 'out'), (12, 610, 710, 'VCA IN', None), (16, 795, 710, 'VCA OUT', None),
    (19, 915, 710, 'PINK', 'out'), (23, 1150, 710, 'WHITE', 'out'), (27, 1270, 710, 'WHEEL', 'out'), (33, 1390, 710, 'SWITCH', 'out'),
    (3, 250, 900, 'SIGNAL IN', 'mono'), (8, 490, 900, 'AMP OUT', None), (15, 730, 900, 'BPF OUT', None),
    (20, 965, 900, 'F-V OUT', None), (28, 1265, 900, 'ENV OUT', None), (34, 1385, 900, 'TRIG OUT', None),
]
ESPK = [(32, 270, 'SIGNAL\nLEVEL', 0.0), (33, 460, 'LOW CUT\nFREQ', 0.49), (34, 640, 'HIGH CUT\nFREQ', 0.76),
        (35, 820, 'CV\nADJUST', 0.78), (36, 1000, 'THRESHOLD\nLEVEL', 0.32)]
VOL = (31, 1210, 280)
RIGHT_POS = {31: (RX(1210), RY(280))}
for n, px, lab, v in ESPK: RIGHT_POS[n] = (RX(px), RY(1010))
LEDS = {'A': (COLX[5] - 1.5, Y(47.0)), 'B': (COLX[6] + 3.2, Y(36.6)), 'C': (RX(1120), RY(828)), 'D': (RX(1385), RY(828)), 'E': (RX(1210), RY(105))}
LEDLAB = {'A': 'RATE', 'B': 'EG', 'C': 'PEAK', 'D': 'TRIG', 'E': ''}


def draw_panel(c, callouts=False, colorjacks=False, jacknums=False, ledcallouts=False, leds_on=('A', 'B', 'C', 'E')):
    # body
    c.setFillColor(PANEL); c.setStrokeColor(EDGE); c.setLineWidth(0.8)
    c.rect(0, 0, PW, PH, stroke=1, fill=1)
    mid = lambda a, b: (COLX[a] + COLX[b]) / 2
    xs = [0.0, mid(0, 1), mid(1, 2), mid(2, 3), mid(3, 4), mid(4, 5), mid(5, 6), mid(6, 7), LW]
    T, B = PH, 0.0
    c.setStrokeColor(INK); c.setLineWidth(0.55)
    def sec(x0, yd0, x1, yd1, title, size=2.2):
        c.rect(x0, Y(yd1), x1 - x0, yd1 - yd0, stroke=1, fill=0)
        text(c, x0 + 0.9, Y(yd0) - size - 0.5, title, size, FONT, anchor='l')
    sec(xs[0], 0, xs[1], 61.5, 'VCO 1'); sec(xs[1], 0, xs[2], 61.5, 'VCO 2')
    sec(xs[0], 61.5, xs[1], PH, 'PORTAMENTO', 1.6); sec(xs[1], 61.5, xs[2], PH, 'MASTER TUNE', 1.6)
    sec(xs[2], 0, xs[3], 45.5, 'VCO MIXER', 1.95); sec(xs[3], 0, xs[4], 45.5, 'HPF'); sec(xs[4], 0, xs[5], 45.5, 'LPF')
    sec(xs[2], 45.5, xs[3], PH, 'FREQ MOD', 1.95); sec(xs[3], 45.5, xs[4], PH, 'HPF MOD', 1.95); sec(xs[4], 45.5, xs[5], PH, 'LPF MOD', 1.95)
    sec(xs[5], 0, xs[7], 33.5, 'VCA'); sec(xs[5], 33.5, xs[6], PH, 'MG'); sec(xs[6], 33.5, xs[7], PH, 'EG 1')
    sec(xs[7], 0, xs[8], PH, 'EG 2')
    c.rect(LW, RY(770), PW - LW, PH - RY(770), stroke=1, fill=0)
    c.rect(LW, 0, PW - LW, RY(770), stroke=1, fill=0)
    # VCA symbol
    cx, cy = (xs[5] + xs[7]) / 2, Y(17.0)
    c.setLineWidth(0.4)
    p = c.beginPath(); p.moveTo(cx - 5, cy + 3.8); p.lineTo(cx + 4.5, cy); p.lineTo(cx - 5, cy - 3.8); p.close(); c.drawPath(p, stroke=1, fill=0)
    c.line(cx - 11, cy, cx - 5, cy); c.line(cx + 4.5, cy, cx + 10, cy); c.line(cx - 0.3, cy - 1.9, cx - 0.3, Y(26.5))
    text(c, cx - 0.3, Y(29.5), 'EG2 & EXT', 1.6)
    # knobs
    for n, kind, col, yd, lab, v, extra in LEFT:
        x, y = COLX[col], Y(yd)
        if kind == 'rot': rotary(c, x, y, KR * 1.05, v, extra, lab)
        else: knob(c, x, y, KR, v, lab, bip=(kind == 'knobb'))
        if callouts:
            if kind == 'rot': callout(c, x + 5.3, y - 0.6, n)
            else: callout(c, x + 4.7, y + 3.7, n)
    # ---- patch panel: block diagram
    blocks = [(195, 320, 'VCO 1'), (360, 485, 'VCO 2'), (530, 600, 'MIX'), (640, 765, 'HPF'), (800, 920, 'LPF'), (950, 1072, 'VCA')]
    for a, b, lab in blocks: box(c, RX(a), RY(185), RX(b), RY(115), lab, 1.9)
    arrow(c, [(RX(600), RY(150)), (RX(640), RY(150))]); arrow(c, [(RX(765), RY(150)), (RX(800), RY(150))]); arrow(c, [(RX(920), RY(150)), (RX(950), RY(150))])
    arrow(c, [(RX(485), RY(160)), (RX(530), RY(160))])
    arrow(c, [(RX(258), RY(115)), (RX(258), RY(95)), (RX(565), RY(95)), (RX(565), RY(115))])
    arrow(c, [(RX(1072), RY(150)), (RX(1130), RY(150))])
    # function blocks between jacks
    box(c, RX(300), RY(495), RX(430), RY(435), 'MG', 1.9)
    box(c, RX(715), RY(500), RX(835), RY(440), 'EG 1', 1.9)
    box(c, RX(880), RY(500), RX(1000), RY(440), 'EG 2', 1.9)
    box(c, RX(310), RY(740), RX(435), RY(680), 'S&H', 1.9)
    box(c, RX(970), RY(740), RX(1085), RY(680), 'NOISE', 1.9)
    # small VCA triangle
    c.setStrokeColor(GREY); c.setLineWidth(0.25)
    p = c.beginPath(); p.moveTo(RX(670), RY(735)); p.lineTo(RX(735), RY(710)); p.lineTo(RX(670), RY(685)); p.close(); c.drawPath(p, stroke=1, fill=0)
    text(c, RX(692), RY(716), 'VCA', 1.3, col=GREY)
    # wires inside blocks
    arrow(c, [(RX(265), RY(710)), (RX(310), RY(710))]); arrow(c, [(RX(435), RY(710)), (RX(468), RY(710))])
    arrow(c, [(RX(365), RY(615)), (RX(365), RY(680))])
    arrow(c, [(RX(630), RY(710)), (RX(670), RY(710))]); arrow(c, [(RX(735), RY(710)), (RX(775), RY(710))])
    arrow(c, [(RX(725), RY(612)), (RX(725), RY(692))])
    # ESP
    text(c, RX(200), RY(808), 'EXTERNAL SIGNAL PROCESSOR', 2.0, FONT, col=GREY, anchor='l')
    box(c, RX(330), RY(925), RX(410), RY(875), 'AMP', 1.6)
    box(c, RX(555), RY(930), RX(680), RY(870), 'BAND PASS\nFILTER', 1.55)
    box(c, RX(790), RY(930), RX(910), RY(870), 'F-V\nCONVERTER', 1.55)
    box(c, RX(1060), RY(930), RX(1180), RY(870), 'ENVELOPE\nFOLLOWER', 1.55)
    for a, b in ((272, 330), (410, 468), (512, 555), (680, 708), (752, 790), (910, 943)):
        arrow(c, [(RX(a), RY(900)), (RX(b), RY(900))])
    arrow(c, [(RX(1180), RY(900)), (RX(1240), RY(900))])
    # jacks
    for num, px, py, lab, role in JACKS:
        st = (role or 'off') if colorjacks else 'plain'
        _jack_draw(c, RX(px), RY(py), lab, st, num if jacknums else None)
    # volume
    n, px, py = VOL
    knob(c, RX(px), RY(py), 4.2, 0.84, None)
    text(c, RX(px), RY(py) + 6.6, 'VOLUME', 2.0)
    text(c, RX(px), RY(py) - 7.3, 'POWER OFF', 1.6)
    if callouts: callout(c, RX(px) + 5.0, RY(py) + 4.6, n)
    # ESP knobs
    for n, px, lab, v in ESPK:
        knob(c, RX(px), RY(1010), 3.2, v, None)
        text(c, RX(px), RY(1010) - 6.4, lab, 1.8)
        if callouts: callout(c, RX(px) + 3.9, RY(1010) + 3.9, n)
    # LEDs
    for k, (x, y) in LEDS.items():
        if k == 'B':
            led(c, x, y, k in leds_on)
        elif k in ('C', 'D'):
            led(c, x, y, k in leds_on); text(c, x - 1.6, y - 0.7, LEDLAB[k], 1.9, anchor='r')
        else: led(c, x, y, k in leds_on, LEDLAB[k] or None)
        if ledcallouts:
            dx, dy = {'A': (-2.4, -1.6), 'B': (0.4, -2.8), 'C': (2.2, 2.0), 'D': (2.2, 2.0), 'E': (2.6, -1.6)}[k]
            c.setFillColor(white); c.setStrokeColor(ACCENT); c.setLineWidth(0.35)
            c.circle(x + dx, y + dy, 1.25, stroke=1, fill=1)
            c.setFillColor(ACCENT); c.setFont(FONTB, 1.7); c.drawCentredString(x + dx, y + dy - 0.6, k)
    # signal arrow to output
    arrow(c, [(RX(1300), RY(185)), (RX(1370), RY(185))])


def _jack_draw(c, x, y, lab, st, num):
    jack(c, x, y, lab, None if st == 'plain' else st, r=1.5, num=num)


def draw_keyboard(c, x0, y0, w, h, keys_white=22):
    kw = w / keys_white
    c.setStrokeColor(HexColor('#555555')); c.setLineWidth(0.25)
    for i in range(keys_white):
        c.setFillColor(HexColor('#f4f2ec')); c.rect(x0 + i*kw, y0, kw, h, stroke=1, fill=1)
    pat = [1, 1, 0, 1, 1, 1, 0]  # black key after C D (not E) F G A (not B)
    for i in range(keys_white - 1):
        if pat[i % 7]:
            c.setFillColor(HexColor('#121212')); c.rect(x0 + (i+1)*kw - kw*0.3, y0 + h*0.38, kw*0.6, h*0.62, stroke=0, fill=1)
