# Twenty Engine - build guide: Ksoloti Core in the MS-20ic
import sys
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.pagesizes import A4
from reportlab.lib.colors import HexColor
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
D = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Cond', D + 'DejaVuSansCondensed.ttf'))
pdfmetrics.registerFont(TTFont('CondB', D + 'DejaVuSansCondensed-Bold.ttf'))
W, H = A4
INK = HexColor('#151515'); GREY = HexColor('#777777'); LIGHT = HexColor('#f2f2f0'); RULE = HexColor('#c8c8c8'); OR = HexColor('#e8542a')
COL = {'Red': HexColor('#d8322a'), 'White': HexColor('#ffffff'), 'Green': HexColor('#2fa35a'), 'Black': HexColor('#151515'),
       'Left': HexColor('#2f6fd6'), 'Right': HexColor('#e8b420'), 'Ground': HexColor('#151515')}
c = canvas.Canvas(sys.argv[1], pagesize=A4)
c.setTitle('Twenty Engine - Ksoloti Core in the MS-20ic'); c.setAuthor('Peter Kohlmetz Moller')
page = [0]

def header():
    page[0] += 1
    c.setFillColor(INK); c.setFont('CondB', 9); c.drawString(18 * mm, H - 12 * mm, 'TWENTY ENGINE')
    c.setFont('Cond', 9); c.setFillColor(GREY); c.drawRightString(W - 18 * mm, H - 12 * mm, 'Build guide · Ksoloti Core · %d' % page[0])
    c.setStrokeColor(RULE); c.setLineWidth(0.5); c.line(18 * mm, H - 14.5 * mm, W - 18 * mm, H - 14.5 * mm)

def wrap(text, font, size, width):
    out, line = [], ''
    for w in text.split():
        t = (line + ' ' + w).strip()
        if pdfmetrics.stringWidth(t, font, size) > width: out.append(line); line = w
        else: line = t
    if line: out.append(line)
    return out

def pinlist(x, y, rows):
    for name, pin in rows:
        col = COL[name]
        c.setFillColor(col); c.setStrokeColor(INK); c.setLineWidth(0.6); c.circle(x + 1.6 * mm, y + 1.2 * mm, 1.6 * mm, stroke=1, fill=1)
        c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(x + 5.5 * mm, y, name)
        c.setFont('Cond', 9.5); c.drawString(x + 23 * mm, y, '→  ' + pin)
        y -= 5.6 * mm
    return y

# ---------------- page 1: recipe
header()
c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, 'Ksoloti Core in the MS-20ic')
c.setFont('Cond', 10.5); c.setFillColor(GREY); c.drawString(18 * mm, H - 37 * mm, 'Eight steps. The pin picture on the next page shows every connection.')

steps = [
    ('Open the MS-20ic', 'Remove the 8 screws on the back and lift the back panel off.', None),
    ('Disconnect the USB cable', 'Find the 4-wire cable that runs from the USB-B daughterboard to the main board, and disconnect it.', None),
    ('Cut the cable to length', 'Cut the cable so that both ends reach the spot on the back of the main board where the Ksoloti will sit, with a little slack.', None),
    ('Strip the wires', 'Strip the outer jacket, then the four coloured wires inside: red, white, green and black.', None),
    ('Connect both halves', 'Solder the wires to the Ksoloti\'s pins, or fit connectors of your choice (DuPont, JST etc.).', 'usb'),
    ('Mount the Ksoloti', 'Put nano tape on the back of the Ksoloti and fit it on the back of the main board.', None),
    ('Install the jacks', 'Drill two holes above the USB-B jack and mount the two output jacks.', None),
    ('Wire the audio', 'Solder LINE OUT left and right to the jacks. Ground is shared by both jacks.', 'audio'),
]
y = H - 52 * mm; tx = 34 * mm; tw = W - 18 * mm - tx
for i, (title, body, extra) in enumerate(steps):
    c.setFillColor(OR); c.circle(23 * mm, y + 1.4 * mm, 4.2 * mm, stroke=0, fill=1)
    c.setFillColor(HexColor('#ffffff')); c.setFont('CondB', 11); c.drawCentredString(23 * mm, y - 0.4 * mm, str(i + 1))
    c.setFillColor(INK); c.setFont('CondB', 11.5); c.drawString(tx, y, title)
    yy = y - 5.6 * mm
    for l in wrap(body, 'Cond', 10, tw):
        c.setFont('Cond', 10); c.setFillColor(INK); c.drawString(tx, yy, l); yy -= 5 * mm
    if extra == 'usb':
        yy -= 1.5 * mm
        c.setFillColor(LIGHT); c.roundRect(tx - 2 * mm, yy - 27 * mm, tw / 2 - 2 * mm, 32 * mm, 2 * mm, stroke=0, fill=1)
        c.roundRect(tx + tw / 2 + 1 * mm, yy - 27 * mm, tw / 2 - 1 * mm, 32 * mm, 2 * mm, stroke=0, fill=1)
        c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(tx + 1 * mm, yy, 'USB-B half → H2 (USB device)')
        pinlist(tx + 1 * mm, yy - 6.5 * mm, [('Red', '5V'), ('White', 'USB PROG D−'), ('Green', 'USB PROG D+'), ('Black', 'GND')])
        xx = tx + tw / 2 + 4 * mm
        c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(xx, yy, 'Main board half → H3 (USB host)')
        pinlist(xx, yy - 6.5 * mm, [('Red', '5V'), ('White', 'USB HOST D−'), ('Green', 'USB HOST D+'), ('Black', 'GND')])
        yy -= 31 * mm
    if extra == 'audio':
        yy -= 1.5 * mm
        c.setFillColor(LIGHT); c.roundRect(tx - 2 * mm, yy - 15.5 * mm, tw * 0.62, 20.5 * mm, 2 * mm, stroke=0, fill=1)
        c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(tx + 1 * mm, yy, 'J1 (line out) → jacks')
        pinlist(tx + 1 * mm, yy - 6.5 * mm, [('Left', 'LINE OUT LEFT CH → left tip'), ('Right', 'LINE OUT RIGHT CH → right tip'), ('Ground', 'LINE OUT GROUND → both')])
        yy -= 19.5 * mm
    y = yy - 6 * mm
c.showPage()

# ---------------- page 2: pin picture
header()
c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, 'Pins')
ir = ImageReader('/mnt/user-data/outputs/Ksoloti_TwentyEngine_pins.png'); iw, ih = ir.getSize()
pw = W - 36 * mm; ph = pw * ih / iw
c.drawImage(ir, 18 * mm, H - 40 * mm - ph, pw, ph)
c.showPage()

# ---------------- page 3: pictures (to come)
header()
c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, 'Pictures')
c.setFillColor(GREY); c.setFont('Cond', 10.5); c.drawString(18 * mm, H - 37 * mm, 'Photos of the finished build.')
c.setFillColor(GREY); c.setFont('Cond', 7.5)
c.drawString(18 * mm, 13 * mm, 'Twenty Engine by Peter Kohlmetz Moller · GPL v3 · Board image: Ksoloti project · MS-20ic is a product of Korg; this guide is not endorsed by Korg.')
c.showPage(); c.save()
