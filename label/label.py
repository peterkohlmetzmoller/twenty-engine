# ESP knob label for the MS-20 iC (Twenty Engine): 130 x 30 mm, square corners.
# Printed 1:1, holes cut out, glued around the five ESP knobs.
# Font: Barlow Semi Condensed (SIL Open Font License) - close to the iC's panel lettering.
import os, sys, math
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
HERE = os.path.dirname(os.path.abspath(__file__))
pdfmetrics.registerFont(TTFont('Cond', HERE + '/BarlowSC-400.ttf'))
pdfmetrics.registerFont(TTFont('CondM', HERE + '/BarlowSC-500.ttf'))
BG = HexColor('#121212'); INK = HexColor('#ecebe6'); GREY = HexColor('#9a9a9a'); RULE = HexColor('#4a4a4a')

MEAS = dict(            # mm
  width=130.0, height=30.0,          # sticker size (Pete)
  span=104.0,                        # first to last knob centre (Pete)
  hole=7.5,                          # PLACEHOLDER: hole for the pot bushing / nut
  skirt=20.0,                        # knob skirt diameter (Pete)
  row_y=15.0,                        # knob centres measured from the bottom edge
)
DRAFT = True

def boxed(c, x, y, t, font, size, padx=1.0, pady=0.75):
    w = pdfmetrics.stringWidth(t, font, size); asc = size * 0.72
    c.setFillColor(INK); c.rect(x - w / 2 - padx * mm, y - pady * mm, w + 2 * padx * mm, asc + 2 * pady * mm, stroke=0, fill=1)
    c.setFillColor(BG); c.setFont(font, size); c.drawCentredString(x, y, t)

def outlined(c, x, y, t, font, size, padx=1.0, pady=0.75):
    w = pdfmetrics.stringWidth(t, font, size); asc = size * 0.72
    c.setStrokeColor(INK); c.setLineWidth(0.35)
    c.rect(x - w / 2 - padx * mm, y - pady * mm, w + 2 * padx * mm, asc + 2 * pady * mm, stroke=1, fill=0)
    c.setFillColor(INK); c.setFont(font, size); c.drawCentredString(x, y, t)

def plain(c, x, y, t, font='Cond', size=8.0, col=None):
    c.setFillColor(col or INK); c.setFont(font, size); c.drawCentredString(x, y, t)

def label(c, ox, oy):
    m = MEAS; W = m['width'] * mm; H = m['height'] * mm; R = m['skirt'] / 2 * mm
    c.setFillColor(BG); c.rect(ox, oy, W, H, stroke=0, fill=1)
    cy = oy + m['row_y'] * mm
    x0 = ox + (W - m['span'] * mm) / 2
    # knob order as on Pete's iC: PLAY MODE first (left) ... SEND last (right)
    xs = [x0 + i * m['span'] * mm / 4 for i in range(5)]
    ytop = cy + R + 1.05 * mm         # arp names (boxed)
    ybot = cy - R - 2.75 * mm         # delay names (plain)
    arp = [None, 'RATE', 'OCTAVES', 'ORDER', 'DELAY AMOUNT']
    dly = [None, 'TIME', 'FEEDBACK', 'WOW / FLUTTER', 'DELAY AMOUNT']
    for i, x in enumerate(xs):
        c.setStrokeColor(RULE); c.setLineWidth(0.35); c.circle(x, cy, R, stroke=1, fill=0)          # knob skirt guide
        c.setFillColor(HexColor('#ffffff')); c.setStrokeColor(INK); c.setLineWidth(0.3)
        c.circle(x, cy, m['hole'] / 2 * mm, stroke=1, fill=1)                                       # cut-out
        if arp[i]: boxed(c, x, ytop, arp[i], 'Cond', 7.0, padx=0.9, pady=0.6)
        if dly[i]: plain(c, x, ybot, dly[i], 'Cond', 7.0)
    # play mode: ranges as arcs (engine zones 0-28 % poly, 28-66 % arp, 66-100 % latch; knob travel 270 deg, 7:30 to 4:30)
    x = xs[0]; ra = R + 0.9 * mm
    def ang(f): return 225.0 - 270.0 * f
    for f0, f1, isarp in ((0.0, 18 / 64, False), (18 / 64, 42 / 64, True), (42 / 64, 1.0, True)):
        a0 = ang(f0) - 2.5; a1 = ang(f1) + 2.5
        rr = ra if isarp else ra - 0.35 * mm
        c.setStrokeColor(INK); c.setLineCap(0); c.setLineWidth(0.9 * mm if isarp else 0.3 * mm)
        p = c.beginPath(); p.arc(x - rr, cy - rr, x + rr, cy + rr, startAng=a1, extent=a0 - a1); c.drawPath(p, stroke=1, fill=0)
    # range names at the ends and top of the arc (no room beside it with 20 mm knobs)
    size = 6.0; asc = size * 0.72
    for t, deg, rl, isarp in (('POLY', 236.0, ra + 3.2 * mm, False), ('ARP', 90.0, ra + 1.9 * mm, True), ('LATCH', -56.0, ra + 3.6 * mm, True)):
        a = math.radians(deg); px = x + math.cos(a) * rl; py = cy + math.sin(a) * rl - asc / 2
        if t != 'ARP': py = ybot + 0.2 * mm          # POLY and LATCH on the delay-name line
        if isarp: boxed(c, px, py, t, 'Cond', size, padx=0.8, pady=0.45)
        else: outlined(c, px, py, t, 'Cond', size, padx=0.8, pady=0.45)
    return W, H

def crop_marks(c, ox, oy, W, H, L=4 * mm, g=1.5 * mm):
    c.setStrokeColor(HexColor('#000000')); c.setLineWidth(0.25)
    for x in (ox, ox + W):
        c.line(x, oy - g, x, oy - g - L); c.line(x, oy + H + g, x, oy + H + g + L)
    for y in (oy, oy + H):
        c.line(ox - g, y, ox - g - L, y); c.line(ox + W + g, y, ox + W + g + L, y)

c = canvas.Canvas(sys.argv[1], pagesize=A4)
ox, oy = 40 * mm, 200 * mm
W, H = label(c, ox, oy); crop_marks(c, ox, oy, W, H)
c.setFillColor(HexColor('#151515')); c.setFont('CondM', 12); c.drawString(20 * mm, 272 * mm, 'Twenty Engine - ESP knob label, 130 x 30 mm')
c.setFont('Cond', 9); c.setFillColor(HexColor('#555555'))
c.drawString(20 * mm, 266 * mm, 'Print at 100 % (no "fit to page") and check the 50 mm bar. Cut at the corner marks; cut the five white holes with a punch or craft knife.')
c.drawString(20 * mm, 261 * mm, 'White boxes = ARP / LATCH mode. Plain text = POLY mode (delay). Matte sticker vinyl or paper + clear laminate works well.')
c.setStrokeColor(HexColor('#151515')); c.setLineWidth(0.6); c.line(20 * mm, 185 * mm, 70 * mm, 185 * mm)
c.setFont('Cond', 8); c.setFillColor(HexColor('#151515')); c.drawString(20 * mm, 181 * mm, '50 mm check')
if DRAFT:
    c.setFillColor(HexColor('#e8542a')); c.setFont('CondM', 10)
    c.drawString(20 * mm, 170 * mm, 'DRAFT: hole size not measured yet (7.5 mm assumed).')
c.showPage(); c.save()
