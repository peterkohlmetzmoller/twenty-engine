# Twenty Engine give-away sticker, 100 x 30 mm. Fonts: Big Shoulders Display Black + Barlow Semi Condensed (both SIL OFL).
import os, sys
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
HERE = os.path.dirname(os.path.abspath(__file__))
pdfmetrics.registerFont(TTFont('Big', HERE + '/ArchivoBlack.ttf'))   # Archivo Black (SIL OFL)
pdfmetrics.registerFont(TTFont('Cond', HERE + '/BarlowSC-400.ttf'))
BLACK = HexColor('#121212'); WHITE = HexColor('#ecebe6'); GREYT = HexColor('#b9b8b3')   # light grey on black
W, H = 100 * mm, 30 * mm
SUB = '8-VOICE POLYPHONIC ENGINE FOR THE MS-20 iC'

def fit(t, font, width):            # font size that makes t exactly this wide
    return width / pdfmetrics.stringWidth(t, font, 1)

def sticker(c, ox, oy, title, dark):
    bg, fg = (BLACK, GREYT) if dark else (WHITE, BLACK)
    c.setFillColor(bg); c.rect(ox, oy, W, H, stroke=0, fill=1)
    c.setStrokeColor(fg); c.setLineWidth(0.9 * mm); m = 2.2 * mm
    c.rect(ox + m, oy + m, W - 2 * m, H - 2 * m, stroke=1, fill=0)      # frame
    inner = W - 2 * m - 8 * mm
    cap = pdfmetrics.getFont('Big').face.capHeight / 1000.0
    s = min(fit(title, 'Big', inner), 13.2 * mm / cap)                   # as wide as possible, cap height max 13.2 mm
    c.setFillColor(fg); c.setFont('Big', s)
    c.drawCentredString(ox + W / 2, oy + 11.2 * mm, title)
    ss = fit(SUB, 'Cond', pdfmetrics.stringWidth(title, 'Big', s))       # subline as wide as the title
    c.setFont('Cond', ss); c.drawCentredString(ox + W / 2, oy + 5.6 * mm, SUB)
    if not dark:                                                        # cut line for the light version
        c.setStrokeColor(HexColor('#bbbbbb')); c.setLineWidth(0.2); c.setDash(1.5, 1.5); c.rect(ox, oy, W, H, stroke=1, fill=0); c.setDash()

c = canvas.Canvas(sys.argv[1], pagesize=A4)
c.setFont('Cond', 11); c.setFillColor(BLACK); c.drawString(20 * mm, 280 * mm, 'Twenty Engine sticker, 100 x 30 mm - print at 100 %')
y = 245 * mm
for title in ('TWENTY ENGINE', '20 ENGINE'):
    sticker(c, 20 * mm, y, title, True); y -= 38 * mm
c.showPage(); c.save()
