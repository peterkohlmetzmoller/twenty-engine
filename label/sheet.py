# Print sheet: ESP label + PW legend at exact size on one A4 page (print at 100 %, no "fit to page").
import os, sys
os.environ.setdefault('STYLE', 'italic'); os.environ.setdefault('INK', '#c4c3be'); os.environ.setdefault('ARPC', '#e8542a')
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
import label as esp, pw_label as pw

c = canvas.Canvas(sys.argv[1], pagesize=A4)
c.setTitle('Twenty Engine - stickers'); c.setAuthor('Peter Kohlmetz Moller')
K = HexColor('#151515'); G = HexColor('#555555')
c.setFillColor(K); c.setFont('CondM', 13); c.drawString(20 * mm, 277 * mm, 'Twenty Engine - stickers for the MS-20ic')
c.setFont('Cond', 9); c.setFillColor(G)
c.drawString(20 * mm, 271 * mm, 'Print at 100 % (actual size, no "fit to page"). Check the 50 mm bar with a ruler before cutting.')
c.drawString(20 * mm, 266.5 * mm, 'Cut at the corner marks. ESP label: punch or cut the five white holes. Orange = Twenty Engine\'s own modes and sounds.')

# ESP label, 130 x 32 mm
ox, oy = 40 * mm, 215 * mm
W, H = esp.label(c, ox, oy); esp.crop_marks(c, ox, oy, W, H)
c.setFillColor(G); c.setFont('Cond', 8); c.drawString(ox, oy - 8 * mm, 'ESP knobs - 130 x 33.5 mm. Widen the holes with a knife if needed.')

# PW legend, scaled to 75 % (34 x 9 mm) after the first test print
PW_SCALE = 0.75
ox, oy = 40 * mm, 175 * mm
c.saveState(); c.translate(ox, oy); c.scale(PW_SCALE, PW_SCALE); pw.sticker(c, 0, 0); c.restoreState()
pw_W, pw_H = pw.W * PW_SCALE, pw.H * PW_SCALE
esp.crop_marks(c, ox, oy, pw_W, pw_H)
c.setFillColor(G); c.setFont('Cond', 8); c.drawString(ox, oy - 8 * mm, 'PW knob - %.0f x %.0f mm' % (pw_W / mm, pw_H / mm))

# 50 mm check bar
c.setStrokeColor(K); c.setLineWidth(0.6); y = 145 * mm
c.line(20 * mm, y, 70 * mm, y); c.line(20 * mm, y - 1.5 * mm, 20 * mm, y + 1.5 * mm); c.line(70 * mm, y - 1.5 * mm, 70 * mm, y + 1.5 * mm)
c.setFillColor(K); c.setFont('Cond', 8); c.drawString(20 * mm, y - 5 * mm, '50 mm')
c.showPage(); c.save()
