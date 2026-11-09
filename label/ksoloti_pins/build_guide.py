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
guide = 'Ksoloti Core'

def header():
    global guide
    page[0] += 1
    c.setFillColor(INK); c.setFont('CondB', 9); c.drawString(18 * mm, H - 12 * mm, 'TWENTY ENGINE')
    c.setFont('Cond', 9); c.setFillColor(GREY); c.drawRightString(W - 18 * mm, H - 12 * mm, 'Build guide · %s · %d' % (guide, page[0]))
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

def recipe(title, sub, steps, boxes):
    header()
    c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, title)
    c.setFont('Cond', 10.5); c.setFillColor(GREY); c.drawString(18 * mm, H - 37 * mm, sub)
    y = H - 52 * mm; tx = 34 * mm; tw = W - 18 * mm - tx
    for i, (stitle, sbody, extra) in enumerate(steps):
        c.setFillColor(OR); c.circle(23 * mm, y + 1.4 * mm, 4.2 * mm, stroke=0, fill=1)
        c.setFillColor(HexColor('#ffffff')); c.setFont('CondB', 11); c.drawCentredString(23 * mm, y - 0.4 * mm, str(i + 1))
        c.setFillColor(INK); c.setFont('CondB', 11.5); c.drawString(tx, y, stitle)
        yy = y - 5.6 * mm
        for l in wrap(sbody, 'Cond', 10, tw):
            c.setFont('Cond', 10); c.setFillColor(INK); c.drawString(tx, yy, l); yy -= 5 * mm
        if extra:
            yy -= 1.5 * mm
            bx = boxes[extra]; n = max(len(r) for _, r in bx); bh = 9 * mm + n * 5.6 * mm
            bw = (tw / len(bx)) - (2 * mm if len(bx) > 1 else 0)
            if len(bx) == 1: bw = tw * 0.62
            for k, (btitle, rows) in enumerate(bx):
                xx = tx + k * (bw + 3 * mm)
                c.setFillColor(LIGHT); c.roundRect(xx - 2 * mm, yy - bh + 4 * mm, bw, bh, 2 * mm, stroke=0, fill=1)
                c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(xx + 1 * mm, yy, btitle)
                pinlist(xx + 1 * mm, yy - 6.5 * mm, rows)
            yy -= bh - 1 * mm
        y = yy - 6 * mm
    c.showPage()

def pins_page(png):
    header()
    c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, 'Pins')
    ir = ImageReader(png); iw, ih = ir.getSize()
    pw = W - 36 * mm; ph = pw * ih / iw
    c.drawImage(ir, 18 * mm, H - 40 * mm - ph, pw, ph)
    c.showPage()

COMMON = [
    ('Open the MS-20ic', 'Remove the 8 screws on the back and lift the back panel off.', None),
    ('Disconnect the USB cable', 'Find the 4-wire cable that runs from the USB-B daughterboard to the main board, and disconnect it.', None),
    ('Cut the cable to length', 'Cut the cable so that both ends reach the spot on the back of the main board where the board will sit, with a little slack.', None),
    ('Strip the wires', 'Strip the outer jacket, then the four coloured wires inside: red, white, green and black.', None),
]
FOOT = 'Twenty Engine by Peter Kohlmetz Moller · GPL v3 · Board images: Ksoloti project, PJRC · MS-20ic is a product of Korg; this guide is not endorsed by Korg.'
guide = 'Ksoloti Core'
recipe('Ksoloti Core in the MS-20ic', 'Eight steps. The pin picture on the next page shows every connection.',
    COMMON + [
    ('Connect both halves', 'Solder the wires to the Ksoloti\'s pins, or fit connectors of your choice (DuPont, JST etc.).', 'usb'),
    ('Mount the Ksoloti', 'Put nano tape on the back of the Ksoloti and fit it on the back of the main board.', None),
    ('Install the jacks', 'Drill two holes above the USB-B jack and mount the two output jacks.', None),
    ('Wire the audio', 'Solder LINE OUT left and right to the jacks. Ground is shared by both jacks.', 'audio')],
    {'usb': [('USB-B half → H2 (USB device)', [('Red', '5V'), ('White', 'USB PROG D−'), ('Green', 'USB PROG D+'), ('Black', 'GND')]),
             ('Main board half → H3 (USB host)', [('Red', '5V'), ('White', 'USB HOST D−'), ('Green', 'USB HOST D+'), ('Black', 'GND')])],
     'audio': [('J1 (line out) → jacks', [('Left', 'LINE OUT LEFT CH → left tip'), ('Right', 'LINE OUT RIGHT CH → right tip'), ('Ground', 'LINE OUT GROUND → both')])]})
pins_page('/mnt/user-data/outputs/Ksoloti_TwentyEngine_pins.png')
# ---------------- page 3: pictures
def pictures(rows):
    header()
    c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, 'Pictures')
    _pics(rows)

def _pics(rows):
  gap = 3 * mm; capH = 6 * mm; x0 = 18 * mm; full = W - 36 * mm
  y = H - 38 * mm
  for row in rows:
    irs = [ImageReader(f) for f, _ in row]
    asp = [ir.getSize()[0] / ir.getSize()[1] for ir in irs]
    h = (full - gap * (len(row) - 1)) / sum(asp)
    x = x0
    for (f, cap), ir, a_ in zip(row, irs, asp):
        w = h * a_
        c.drawImage(ir, x, y - h, w, h)
        c.setFillColor(GREY); c.setFont('Cond', 7.6)
        for k, l in enumerate(wrap(cap, 'Cond', 7.6, w)):
            c.drawString(x, y - h - 3.6 * mm - k * 3.2 * mm, l)
        x += w + gap
    y -= h + capH + gap
  c.setFillColor(GREY); c.setFont('Cond', 7.5)
  c.drawString(18 * mm, 10 * mm, FOOT)
  c.showPage()

KROWS = [
    [('connectors.jpg', 'Inside: the PH2.0 connector from the USB-B board, and the USB host pins'), ('cable.jpg', 'The internal USB cable and its PH2.0 plug')],
    [('strip.jpg', 'Cut and stripped: red, white, green, black'), ('mount.jpg', 'Nano tape on the back of the Ksoloti'), ('h3.jpg', 'USB host wires on H3')],
    [('holes.jpg', 'Two holes above the USB-B jack'), ('jacks.jpg', 'Jacks installed')],
]
pictures(KROWS)

# ================= Teensy 4.1 + audio shield =================
guide = 'Teensy 4.1'
recipe('Teensy 4.1 in the MS-20ic', 'Nine steps. The pin picture on the next page shows every connection.',
    [('Open the MS-20ic', 'Remove the 8 screws on the back and lift the back panel off.', None),
    ('Disconnect the USB cable', 'Find the 4-wire cable that runs from the USB-B daughterboard to the main board, and disconnect it from the USB-B board.', None),
    ('Cut the cable to length', 'Cut the cable so that it reaches from the main board to the spot where the Teensy will sit, with a little slack.', None),
    ('Strip the wires', 'Strip the outer jacket, then the four coloured wires inside: red, white, green and black.', None),
    ('Main board cable → USB host pads', 'Solder the four wires to the Teensy\'s USB host pads, or fit connectors of your choice (DuPont, JST etc.).', None),
    ('USB-B board → micro-USB', 'Find a micro-USB to USB-A cable. Cut off the USB-A end and strip the wires. Desolder the PH2.0 connector on the USB-B board and solder the four wires to its pads, colour to colour. Plug the micro-USB end into the Teensy. Alternatively, buy a PH2.0 to micro-USB cable.', 'usb'),
    ('Mount the Teensy', 'Stack the audio shield on the Teensy. Put nano tape on the back and fit it on the back of the main board.', None),
    ('Install the jacks', 'Drill two holes above the USB-B jack and mount the two output jacks.', None),
    ('Wire the audio', 'Solder the audio shield\'s LINE OUT to the jacks. Ground is shared by both jacks.', 'audio')],
    {'usb': [('Main board cable → USB host pads (step 5)', [('Red', '5V'), ('White', 'D−'), ('Green', 'D+'), ('Black', 'GND')]),
             ('Micro-USB cable → USB-B board (step 6)', [('Red', '5V'), ('White', 'D−'), ('Green', 'D+'), ('Black', 'GND')])],
     'audio': [('Audio shield LINE OUT → jacks', [('Left', 'L → left tip'), ('Right', 'R → right tip'), ('Ground', 'G → both')])]})
pins_page('/mnt/user-data/outputs/Teensy_TwentyEngine_pins.png')
pictures([
    [('t_wires.jpg', 'Cut and stripped: red, white, green, black'), ('t_host.jpg', 'Main board cable on the USB host pads'), ('t_shield.jpg', 'Audio shield stacked on the Teensy')],
    [('ph_teensy_host.jpg', 'USB host pads on the Teensy 4.1, with wires'), ('ph_shield.jpg', 'LINE OUT L, G, R on the audio shield')],
    [('holes.jpg', 'Two holes above the USB-B jack'), ('jacks.jpg', 'Jacks installed')],
])

# ================= optional ready-made cables =================
guide = 'Cables'
header()
c.setFillColor(INK); c.setFont('Cond', 22); c.drawString(18 * mm, H - 30 * mm, 'Optional: ready-made USB cables')
c.setFont('Cond', 10.5); c.setFillColor(GREY)
c.drawString(18 * mm, H - 37 * mm, 'Plug-in cables with a PH2.0 4-pin connector make the USB connections easier.')
c.drawString(18 * mm, H - 42.5 * mm, 'Available on AliExpress and similar shops.')
c.drawString(18 * mm, H - 48 * mm, 'Check that the pin order matches the MS-20ic\'s own cable (5V, D−, D+, GND) before you plug it in.')

def section(y, title, search, imgs):
    c.setFillColor(INK); c.setFont('CondB', 13); c.drawString(18 * mm, y, title)
    c.setFillColor(LIGHT); c.roundRect(18 * mm, y - 15 * mm, W - 36 * mm, 11 * mm, 2 * mm, stroke=0, fill=1)
    c.setFillColor(GREY); c.setFont('Cond', 9); c.drawString(22 * mm, y - 8.2 * mm, 'Search for:')
    c.setFillColor(INK); c.setFont('CondB', 10); c.drawString(40 * mm, y - 8.2 * mm, search)
    irs = [ImageReader(f) for f in imgs]
    asp = [ir.getSize()[0] / ir.getSize()[1] for ir in irs]
    gap = 4 * mm; full = W - 36 * mm
    h = min(70 * mm, (full - gap * (len(irs) - 1)) / sum(asp)); x = 18 * mm; top = y - 19 * mm
    for ir, a_ in zip(irs, asp):
        w = h * a_; c.drawImage(ir, x, top - h, w, h); x += w + gap
    return top - h - 12 * mm

y = H - 61 * mm
y = section(y, 'Ksoloti Core · USB-C to PH2.0', '"USB-C to PH2.0 4P cable"  ·  "Type-C 4 pin PH2.0 cable"', ['ali_usbc.png'])
y = section(y, 'Teensy 4.1 · micro-USB to PH2.0', '"Micro USB to PH2.0 4P cable"  ·  "Micro USB 4 pin PH2.0 cable"', ['ali_micro_angled.png', 'ali_micro_straight.png'])
c.setFillColor(GREY); c.setFont('Cond', 7.5); c.drawString(18 * mm, 10 * mm, 'Product photos: AliExpress listings.')
c.showPage()
c.save()
