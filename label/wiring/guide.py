# Twenty Engine - wiring guide for the MS-20 iC (Teensy 4.1 + audio shield, or Ksoloti Core)
import sys
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.pagesizes import A4
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
D = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Cond', D + 'DejaVuSansCondensed.ttf'))
pdfmetrics.registerFont(TTFont('CondB', D + 'DejaVuSansCondensed-Bold.ttf'))
W, H = A4
INK = HexColor('#151515'); GREY = HexColor('#777777'); LIGHT = HexColor('#f2f2f0'); RULE = HexColor('#c8c8c8')
OR = HexColor('#e8542a'); PCB = HexColor('#2f8f4e'); PCBD = HexColor('#1f6b38'); PAD = HexColor('#d9b44a')
C5V = HexColor('#d8322a'); CDM = HexColor('#ffffff'); CDP = HexColor('#2fa35a'); CGND = HexColor('#151515')
CL = HexColor('#2f6fd6'); CR = HexColor('#e8b420'); CASE = HexColor('#3a3a3c')
WIRES = [('5V', C5V), ('D-', CDM), ('D+', CDP), ('GND', CGND)]
c = canvas.Canvas(sys.argv[1], pagesize=A4)
c.setTitle('Twenty Engine - wiring guide for the MS-20 iC'); c.setAuthor('Peter Kohlmetz Moller')
page = [0]

def header(title, sub=None):
    page[0] += 1
    c.setFillColor(INK); c.setFont('CondB', 9); c.drawString(18 * mm, H - 12 * mm, 'TWENTY ENGINE')
    c.setFont('Cond', 9); c.setFillColor(GREY); c.drawRightString(W - 18 * mm, H - 12 * mm, 'Wiring guide  ·  %d' % page[0])
    c.setStrokeColor(RULE); c.setLineWidth(0.5); c.line(18 * mm, H - 14.5 * mm, W - 18 * mm, H - 14.5 * mm)
    c.setFillColor(INK); c.setFont('Cond', 20); c.drawString(18 * mm, H - 27 * mm, title)
    if sub: c.setFont('Cond', 10); c.setFillColor(GREY); c.drawString(18 * mm, H - 33 * mm, sub)

def text(x, y, lines, size=9.2, lead=12.6, font='Cond', col=INK):
    for i, l in enumerate(lines):
        yy = y - i * lead; c.setFillColor(col)
        if l.startswith('**') and '**' in l[2:]:
            b, rest = l[2:].split('**', 1)
            c.setFont('CondB', size); c.drawString(x, yy, b)
            c.setFont(font, size); c.drawString(x + pdfmetrics.stringWidth(b, 'CondB', size), yy, rest)
        else:
            c.setFont(font, size); c.drawString(x, yy, l)
    return y - len(lines) * lead

def wire(pts, col, w=1.6):
    if col == CDM:   # white wire: grey outline so it shows on paper
        c.setStrokeColor(HexColor('#9a9a9a')); c.setLineWidth(w + 0.9); c.setLineCap(1); c.setLineJoin(1)
        p = c.beginPath(); p.moveTo(*pts[0]); [p.lineTo(*q) for q in pts[1:]]; c.drawPath(p, stroke=1, fill=0)
    c.setStrokeColor(col); c.setLineWidth(w); c.setLineCap(1); c.setLineJoin(1)
    p = c.beginPath(); p.moveTo(*pts[0]); [p.lineTo(*q) for q in pts[1:]]; c.drawPath(p, stroke=1, fill=0)

def pcb(x, y, w, h, label=None, col=PCB):
    c.setFillColor(col); c.setStrokeColor(PCBD); c.setLineWidth(0.8); c.roundRect(x, y, w, h, 2 * mm, stroke=1, fill=1)
    if label: c.setFillColor(white); c.setFont('CondB', 9); c.drawString(x + 3 * mm, y + h - 6 * mm, label)

def pad(x, y, lab=None, below=True, col=PAD, r=1.5 * mm):
    c.setFillColor(col); c.setStrokeColor(HexColor('#8a6d1c')); c.setLineWidth(0.4); c.circle(x, y, r, stroke=1, fill=1)
    c.setFillColor(INK); c.circle(x, y, r * 0.38, stroke=0, fill=1)
    if lab:
        c.setFillColor(white); c.setFont('CondB', 6.8); c.drawCentredString(x, y - 4.6 * mm if below else y + 3 * mm, lab)

def box(x, y, w, h, title, lines, fill=LIGHT):
    c.setFillColor(fill); c.roundRect(x, y, w, h, 2.5 * mm, stroke=0, fill=1)
    c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(x + 4 * mm, y + h - 6.5 * mm, title)
    text(x + 4 * mm, y + h - 12 * mm, lines, size=8.6, lead=11.6)

def usbport(x, y, kind, label, right=True):
    w, h = (8 * mm, 5 * mm) if kind == 'C' else (7.5 * mm, 3.6 * mm) if kind == 'micro' else (11 * mm, 10 * mm)
    c.setFillColor(HexColor('#c9c9c9')); c.setStrokeColor(HexColor('#6d6d6d')); c.setLineWidth(0.6)
    c.roundRect(x, y - h / 2, w, h, 1.6 * mm if kind == 'C' else 0.8 * mm, stroke=1, fill=1)
    c.setFillColor(INK); c.setFont('CondB', 7); c.drawCentredString(x + w / 2, y - h / 2 - 3.6 * mm, label)
    return w

def jack(x, y, lab, col):
    c.setFillColor(HexColor('#2b2b2b')); c.circle(x, y, 5 * mm, stroke=0, fill=1)
    c.setFillColor(HexColor('#bdbdbd')); c.circle(x, y, 3.2 * mm, stroke=0, fill=1)
    c.setFillColor(INK); c.circle(x, y, 1.6 * mm, stroke=0, fill=1)
    c.setFillColor(col); c.setFont('CondB', 9); c.drawCentredString(x, y + 7 * mm, lab)

def lug(x, y, lab):
    c.setFillColor(HexColor('#bdbdbd')); c.setStrokeColor(HexColor('#6d6d6d')); c.setLineWidth(0.5)
    c.rect(x - 1.2 * mm, y - 3 * mm, 2.4 * mm, 6 * mm, stroke=1, fill=1)
    c.setFillColor(GREY); c.setFont('Cond', 6.5); c.drawCentredString(x, y - 6 * mm, lab)

def legend(x, y):
    items = WIRES + [('LEFT', CL), ('RIGHT', CR)]
    for i, (n, col) in enumerate(items):
        xx = x + i * 26 * mm; wire([(xx, y), (xx + 9 * mm, y)], col, 2.2)
        c.setFillColor(INK); c.setFont('Cond', 8); c.drawString(xx + 11 * mm, y - 2.8, n)

# ============================================================ page 1: overview
header('Wiring the MS-20 iC', 'Twenty Engine inside the iC: Teensy 4.1 + audio shield, or Ksoloti Core')
y = text(18 * mm, H - 44 * mm, [
    'The engine board sits inside the iC, between the iC\'s own USB socket and its main board. It does two jobs at once:',
    '•  it talks to the iC\'s keys, knobs and patch cables through its USB HOST port, exactly as a computer would,',
    '•  and it passes everything on to your computer through the iC\'s original USB socket (its USB DEVICE port).',
    'Audio comes out of two new jacks drilled above the iC\'s USB socket.'])
# topology diagram
y0 = H - 112 * mm
c.setFillColor(LIGHT); c.roundRect(18 * mm, y0 - 34 * mm, W - 36 * mm, 64 * mm, 3 * mm, stroke=0, fill=1)
# computer
c.setFillColor(INK); c.roundRect(22 * mm, y0 + 4 * mm, 24 * mm, 16 * mm, 2 * mm, stroke=0, fill=1)
c.setFillColor(white); c.setFont('CondB', 8); c.drawCentredString(34 * mm, y0 + 11 * mm, 'COMPUTER')
c.setFillColor(GREY); c.setFont('Cond', 7); c.drawCentredString(34 * mm, y0 + 1 * mm, '(optional)')
wire([(46 * mm, y0 + 12 * mm), (56 * mm, y0 + 12 * mm)], INK, 2.4)
# iC outline
c.setStrokeColor(CASE); c.setDash(3, 2); c.setLineWidth(0.8); c.roundRect(55 * mm, y0 - 30 * mm, 135 * mm, 57 * mm, 3 * mm, stroke=1, fill=0); c.setDash()
c.setFillColor(CASE); c.setFont('CondB', 8); c.drawString(58 * mm, y0 + 22 * mm, 'INSIDE THE MS-20 iC')
pcb(57 * mm, y0 + 2 * mm, 24 * mm, 18 * mm, None)
usbport(58 * mm, y0 + 9 * mm, 'B', '')
c.setFillColor(white); c.setFont('CondB', 8); c.drawString(63 * mm, y0 + 15 * mm, 'USB board')
pcb(57 * mm, y0 - 27 * mm, 46 * mm, 18 * mm, 'iC main board')
pcb(124 * mm, y0 - 14 * mm, 50 * mm, 30 * mm, 'Teensy or Ksoloti', col=HexColor('#3a7bd5'))
c.setFillColor(white); c.setFont('Cond', 7.5)
c.drawString(127 * mm, y0 + 3 * mm, 'USB DEVICE ← computer'); c.drawString(127 * mm, y0 - 5 * mm, 'USB HOST → iC main board')
c.drawString(127 * mm, y0 - 12 * mm, 'LINE OUT → audio jacks')
# cut cable halves
for i, (n, col) in enumerate(WIRES):
    wire([(81 * mm, y0 + 6 * mm + i * 1.6 * mm), (124 * mm, y0 + 4 * mm + i * 1.6 * mm)], col, 1.2)
    wire([(103 * mm, y0 - 21 * mm + i * 1.6 * mm), (112 * mm, y0 - 21 * mm + i * 1.6 * mm), (124 * mm, y0 - 6 * mm + i * 1.6 * mm)], col, 1.2)
c.setFillColor(OR); c.setFont('CondB', 8); c.drawString(88 * mm, y0 + 14 * mm, 'cable half 1')
c.drawString(84 * mm, y0 - 31 * mm + 34 * mm, 'cable half 2')
# jacks
jack(182 * mm, y0 + 12 * mm, 'L', CL); jack(182 * mm, y0 - 6 * mm, 'R', CR)
wire([(174 * mm, y0 - 10 * mm), (177 * mm, y0 - 10 * mm), (177 * mm, y0 + 12 * mm)], CL, 1.4)
wire([(174 * mm, y0 - 12 * mm), (179 * mm, y0 - 12 * mm), (179 * mm, y0 - 6 * mm)], CR, 1.4)
c.setFillColor(GREY); c.setFont('Cond', 7); c.drawCentredString(182 * mm, y0 - 17 * mm, 'new jacks')
# what you need
box(18 * mm, 40 * mm, 84 * mm, 74 * mm, 'You need', [
    'Teensy 4.1 + audio shield (Rev D),', '   or a Ksoloti Core',
    'Two 6.35 mm (or 3.5 mm) mono jacks', 'Thin wire, heat-shrink tubing',
    'Soldering iron, wire stripper, drill', 'Strong double-sided nano tape',
    'Kapton or insulating tape', 'Teensy: a micro-USB plug or cable to cut',
    'Ksoloti: two USB-C plugs, cables to cut,', '   or USB-C to PH2.0 cables (see page 4)'])
box(108 * mm, 40 * mm, 84 * mm, 74 * mm, 'Before you start', [
    'Unplug the iC. Open it on a soft surface.', 'Take photos of every cable before you',
    '   unplug anything.', 'Every connection can be soldered, or made',
    '   with DuPont jumper connectors: no', '   soldering on the engine board needed.',
    'Opening and modifying the iC is at your', '   own risk and may void its warranty.',
    '**Colour code used in this guide:**'])
legend(22 * mm, 30 * mm)
c.showPage()

# ============================================================ page 2: the USB cable
header('Step 1 · The iC\'s USB cable', 'Find it, cut it, prepare both halves')
text(18 * mm, H - 44 * mm, [
    'The iC\'s USB socket sits on a small board in the middle of the back panel. A 4-wire cable runs from that board',
    'to the big green main board, where it plugs into a small white PH2.0 connector.'])
y0 = H - 100 * mm
pcb(20 * mm, y0 - 12 * mm, 34 * mm, 26 * mm, 'USB board'); usbport(14 * mm, y0, 'B', 'to computer')
pcb(150 * mm, y0 - 18 * mm, 42 * mm, 38 * mm, 'iC main board')
c.setFillColor(white); c.roundRect(146 * mm, y0 - 5 * mm, 9 * mm, 10 * mm, 1 * mm, stroke=0, fill=1)
c.setFillColor(INK); c.setFont('CondB', 6.5); c.drawCentredString(150.5 * mm, y0 - 9 * mm, 'PH2.0')
for i, (n, col) in enumerate(WIRES):
    yy = y0 + 3 * mm - i * 2.2 * mm
    wire([(54 * mm, yy), (96 * mm, yy)], col, 1.8); wire([(104 * mm, yy), (146 * mm, yy)], col, 1.8)
# scissors mark
c.setStrokeColor(OR); c.setLineWidth(1.2); c.setDash(2, 1.5); c.line(100 * mm, y0 + 9 * mm, 100 * mm, y0 - 9 * mm); c.setDash()
c.setFillColor(OR); c.setFont('CondB', 9); c.drawCentredString(100 * mm, y0 + 12 * mm, 'CUT HERE (about the middle)')
c.setFont('CondB', 8.5); c.drawCentredString(75 * mm, y0 - 14 * mm, 'HALF 1  ·  computer side')
c.drawCentredString(125 * mm, y0 - 14 * mm, 'HALF 2  ·  iC main board side')
c.setFillColor(GREY); c.setFont('Cond', 7.8)
c.drawCentredString(75 * mm, y0 - 18.5 * mm, 'goes to the engine\'s USB DEVICE port')
c.drawCentredString(125 * mm, y0 - 18.5 * mm, 'goes to the engine\'s USB HOST port')
# colour table
ty = H - 152 * mm
c.setFillColor(LIGHT); c.roundRect(18 * mm, ty - 46 * mm, 84 * mm, 52 * mm, 2.5 * mm, stroke=0, fill=1)
c.setFillColor(INK); c.setFont('CondB', 9.5); c.drawString(22 * mm, ty, 'USB colour code')
for i, (n, col) in enumerate(WIRES):
    yy = ty - 9 * mm - i * 8 * mm
    wire([(22 * mm, yy), (36 * mm, yy)], col, 3)
    c.setFillColor(INK); c.setFont('CondB', 9); c.drawString(40 * mm, yy - 3, n)
    c.setFont('Cond', 8.6); c.drawString(54 * mm, yy - 3, ['power, 5 V', 'data minus', 'data plus', 'ground'][i])
c.setFillColor(GREY); c.setFont('Cond', 7.6); c.drawString(22 * mm, ty - 43 * mm, 'Bare braid (shield): fold back, or to a GND pin.')
box(108 * mm, ty - 46 * mm, 84 * mm, 52 * mm, 'Important', [
    'Check the colours with a multimeter', '   (beep test) before you solder: not every',
    '   cable follows the standard.', '**Never join the two red wires.**',
    'Half 1 brings 5 V from the computer;', 'the engine board feeds the iC from its',
    '   host port on half 2.', 'Strip ~4 mm, twist, tin, heat-shrink.'])
text(18 * mm, ty - 58 * mm, ['**Why cut instead of unplug?**',
    'Half 2 keeps the iC\'s own PH2.0 plug, so the main board end stays untouched and can be restored',
    'by joining the halves again. Half 1 keeps the USB board connected to the original back-panel socket.'])
legend(22 * mm, 22 * mm)
c.showPage()

# ============================================================ page 3: Teensy
header('Step 2a · Teensy 4.1 + audio shield', 'USB host pads, micro-USB device port and the audio shield\'s LINE OUT pads')
y0 = H - 112 * mm
# Teensy drawn from below (host pads side)
pcb(20 * mm, y0 - 16 * mm, 110 * mm, 34 * mm, None)
c.setFillColor(white); c.setFont('CondB', 9); c.drawString(80 * mm, y0 + 3 * mm, 'Teensy 4.1')
for k in range(24):
    pad(26 * mm + k * 4.4 * mm, y0 + 14 * mm, None, r=1.1 * mm); pad(26 * mm + k * 4.4 * mm, y0 - 12 * mm, None, r=1.1 * mm)
usbport(12.5 * mm, y0, 'micro', 'USB DEVICE')
# host pads
hx = 32 * mm; c.setFillColor(white); c.setFont('CondB', 7); c.drawString(hx - 1.5 * mm, y0 + 6.5 * mm, 'USB HOST pads')
for i, n in enumerate(['5V', 'D-', 'D+', 'GND', 'GND']): pad(hx + i * 7.5 * mm, y0 + 1 * mm, n, below=True)
# half 2 to host pads
cols = [C5V, CDM, CDP, CGND]
for i, col in enumerate(cols):
    px = hx + i * 7.5 * mm
    wire([(px, y0 - 1 * mm), (px, y0 - 30 * mm - i * 2 * mm), (150 * mm, y0 - 30 * mm - i * 2 * mm)], col, 1.5)
c.setFillColor(OR); c.setFont('CondB', 8.5); c.drawString(152 * mm, y0 - 33 * mm, 'HALF 2 → iC main board')
# half 1 to micro-USB plug
for i, col in enumerate(cols):
    wire([(10 * mm, y0 - 1.5 * mm + i * 1 * mm), (5 * mm, y0 - 1.5 * mm + i * 1 * mm)], col, 1.0)
c.setFillColor(OR); c.setFont('CondB', 8.5); c.drawString(14 * mm, y0 + 24 * mm, 'HALF 1 → a micro-USB plug in the Teensy\'s socket')
c.setFillColor(GREY); c.setFont('Cond', 7.6)
c.drawString(14 * mm, y0 + 20 * mm, '(splice half 1 to a cut micro-USB cable, same colours; the Teensy\'s device port has no solder pads)')
# audio shield
ay = H - 200 * mm
pcb(20 * mm, ay - 18 * mm, 62 * mm, 36 * mm, 'Audio shield Rev D  (underside)')
c.setFillColor(white); c.setFont('CondB', 7); c.drawString(26 * mm, ay + 4 * mm, 'LINE OUT')
lx = 46 * mm
for i, n in enumerate(['L', 'G', 'R']): pad(lx + i * 6 * mm, ay + 5 * mm, n, below=True)
c.setFillColor(white); c.setFont('CondB', 7); c.drawString(26 * mm, ay - 10 * mm, 'headphone jack: do NOT use')
jx = 150 * mm; jack(jx, ay + 6 * mm, 'L', CL); jack(jx + 22 * mm, ay + 6 * mm, 'R', CR)
lug(jx, ay - 6 * mm, 'tip'); lug(jx + 4 * mm, ay - 6 * mm, 'sleeve'); lug(jx + 22 * mm, ay - 6 * mm, 'tip'); lug(jx + 26 * mm, ay - 6 * mm, 'sleeve')
wire([(lx, ay + 5 * mm), (lx, ay - 22 * mm), (jx, ay - 22 * mm), (jx, ay - 9 * mm)], CL, 1.5)
wire([(lx + 12 * mm, ay + 5 * mm), (lx + 12 * mm, ay - 26 * mm), (jx + 22 * mm, ay - 26 * mm), (jx + 22 * mm, ay - 9 * mm)], CR, 1.5)
wire([(lx + 6 * mm, ay + 5 * mm), (lx + 6 * mm, ay - 30 * mm), (jx + 4 * mm, ay - 30 * mm), (jx + 4 * mm, ay - 9 * mm)], CGND, 1.5)
wire([(jx + 4 * mm, ay - 30 * mm), (jx + 26 * mm, ay - 30 * mm), (jx + 26 * mm, ay - 9 * mm)], CGND, 1.5)
box(18 * mm, 18 * mm, 84 * mm, 44 * mm, 'Teensy notes', [
    'Host pad order as printed: 5V, D−, D+, GND,', '   GND. Use either GND pad.',
    'USB Type in the Arduino IDE: Serial + MIDI.', 'Audio: use the LINE OUT pads, not the',
    '   headphone jack ("do not short VGND to', '   GND" is printed on the shield).'])
box(108 * mm, 18 * mm, 84 * mm, 44 * mm, 'Audio jacks', [
    'Ground (G) goes to the sleeve of BOTH', '   jacks.',
    'L to the tip of the left jack,', 'R to the tip of the right jack.',
    'Mono jacks are fine; with stereo jacks,', '   leave the ring lug unconnected.'])
c.showPage()

# ============================================================ page 4: Ksoloti
header('Step 2b · Ksoloti Core', 'Two USB-C ports and the 6-pin LINE header')
y0 = H - 104 * mm
pcb(40 * mm, y0 - 20 * mm, 110 * mm, 44 * mm, 'Ksoloti Core')
usbport(32 * mm, y0 + 10 * mm, 'C', 'USB DEVICE (C)')
usbport(32 * mm, y0 - 8 * mm, 'C', 'USB HOST (C)')
for i, col in enumerate([C5V, CDM, CDP, CGND]):
    wire([(32 * mm, y0 + 8.5 * mm + i * 1 * mm), (20 * mm, y0 + 8.5 * mm + i * 1 * mm)], col, 1.0)
    wire([(32 * mm, y0 - 9.5 * mm + i * 1 * mm), (20 * mm, y0 - 9.5 * mm + i * 1 * mm)], col, 1.0)
c.setFillColor(OR); c.setFont('CondB', 8.5)
c.drawRightString(19 * mm, y0 + 14 * mm, 'HALF 1'); c.drawRightString(19 * mm, y0 - 4 * mm, 'HALF 2')
# LINE header
hx = 82 * mm; c.setFillColor(white); c.setFont('CondB', 7); c.drawString(hx - 2 * mm, y0 + 14 * mm, 'LINE header (6 pins)')
labs = ['IN L', 'IN G', 'IN R', 'OUT R', 'OUT G', 'OUT L']
for i, n in enumerate(labs):
    c.setFillColor(HexColor('#1a1a1a')); c.rect(hx + i * 10 * mm - 2.4 * mm, y0 + 3 * mm, 4.8 * mm, 4.8 * mm, stroke=0, fill=1)
    pad(hx + i * 10 * mm, y0 + 5.4 * mm, None, r=1.1 * mm)
    c.setFillColor(white if i >= 3 else HexColor('#9fd3ae')); c.setFont('CondB', 6.2); c.drawCentredString(hx + i * 10 * mm, y0 - 1 * mm, n)
oL, oG, oR = hx + 5 * 10 * mm, hx + 4 * 10 * mm, hx + 3 * 10 * mm
jx = 168 * mm; jack(jx, y0 + 8 * mm, 'L', CL); jack(jx + 20 * mm, y0 + 8 * mm, 'R', CR)
wire([(oL, y0 + 5.4 * mm), (oL, y0 - 26 * mm), (jx, y0 - 26 * mm), (jx, y0 + 3 * mm)], CL, 1.5)
wire([(oR, y0 + 5.4 * mm), (oR, y0 - 30 * mm), (jx + 20 * mm, y0 - 30 * mm), (jx + 20 * mm, y0 + 3 * mm)], CR, 1.5)
wire([(oG, y0 + 5.4 * mm), (oG, y0 - 34 * mm), (jx + 24 * mm, y0 - 34 * mm), (jx + 24 * mm, y0 + 3 * mm)], CGND, 1.5)
wire([(jx + 4 * mm, y0 - 34 * mm), (jx + 4 * mm, y0 + 3 * mm)], CGND, 1.5)
c.setFillColor(GREY); c.setFont('Cond', 7.4); c.drawString(jx - 6 * mm, y0 - 40 * mm, 'G to the sleeve of both jacks')
box(18 * mm, 76 * mm, 84 * mm, 52 * mm, 'USB HOST: header H3 pins (easiest)', [
    'The Ksoloti has its USB host on pins too', '   (header H3, see the next page):',
    'red → 5.0V, white → USB HOST D−,', 'green → USB HOST D+, black → GND.',
    'DuPont jumpers fit directly: no USB-C', '   plug needed for half 2.',
    'Half 1 → a USB-C plug in USB DEVICE.'])
box(108 * mm, 76 * mm, 84 * mm, 52 * mm, 'Alternative: USB-C to PH2.0 cables', [
    'Ready-made USB-C to PH2.0 4-pin cables', '   exist (AliExpress etc.): one can go from',
    '   USB HOST (C) into the iC main board\'s', '   PH2.0 socket, with no cutting there.',
    'For half 1: cut a USB 2.0 USB-C cable and', '   splice colour to colour.',
    'Check every pin order with a meter first!'])
box(18 * mm, 18 * mm, 174 * mm, 50 * mm, 'Ksoloti notes', [
    'LINE header = J1, a row of 6 pins near the bottom-left corner. From the square pin 1: IN L, IN GND,',
    '   IN R, OUT R, OUT GND, OUT L (from Ksoloti\'s official pin sheet, v0.6 and earlier; see next page).',
    'Newer boards (v0.7+): check Ksoloti\'s v0.7 pin sheet, the layout may differ.',
    'Use LINE OUT, not the 3-pin headphone header. DuPont jumper wires fit the header pins directly.',
    'The Ksoloti needs firmware 1.1.0 or newer (see the Twenty Engine manual, section 18).'])
c.showPage()

# ============================================================ real pictures
def photo(path, x, y, w, cap):
    from reportlab.lib.utils import ImageReader
    ir = ImageReader(path); iw, ih = ir.getSize(); h = w * ih / iw
    c.drawImage(ir, x, y - h, w, h)
    c.setStrokeColor(RULE); c.setLineWidth(0.5); c.rect(x, y - h, w, h, stroke=1, fill=0)
    c.setFillColor(GREY); c.setFont('Cond', 7.8); c.drawString(x, y - h - 4.5 * mm, cap)
    return y - h
def tag(x, y, t, col, tx, ty):
    wire([(tx, ty), (x, y)], col, 1.2)
    c.setFillColor(col); c.circle(x, y, 1.1 * mm, stroke=0, fill=1)
    tw = pdfmetrics.stringWidth(t, 'CondB', 8) + 3 * mm
    c.setFillColor(col if col != CDM else HexColor('#9a9a9a')); c.roundRect(tx - tw / 2, ty - 2 * mm, tw, 5 * mm, 1.2 * mm, stroke=0, fill=1)
    c.setFillColor(white if col not in (CDM, CR) else INK); c.setFont('CondB', 8); c.drawCentredString(tx, ty - 0.6 * mm, t)

header('Real pictures · Teensy 4.1 + audio shield', 'The actual pads, as they look on the boards')
x0 = 18 * mm; pw = W - 36 * mm
b = photo('ph_teensy_host.jpg', x0, H - 40 * mm, pw, 'Teensy 4.1, underside: the five USB HOST pads printed 5V, D−, D+, GND, GND (here with wires soldered).')
# photo is 1000x600 px drawn at pw wide: pad positions measured on the photo
sc = pw / 1000.0; top = H - 40 * mm
for t, px, col in (('GND', 275, CGND), ('GND', 380, CGND), ('D+', 480, CDP), ('D−', 585, CDM), ('5V', 700, C5V)):
    tag(x0 + px * sc, top - 437 * sc, t, col, x0 + px * sc, top - 300 * sc)
b2 = photo('ph_shield.jpg', x0, b - 14 * mm, pw, 'Audio shield Rev D, underside: LINE OUT L, G, R (soldered here). Do not use the headphone jack.')
sc2 = pw / 1182.0; top2 = b - 14 * mm
for t, px, col in (('LEFT', 202, CL), ('GND', 320, CGND), ('RIGHT', 399, CR)):
    tag(x0 + px * sc2, top2 - 217 * sc2, t, col, x0 + px * sc2 + (-16 if t == 'LEFT' else 16 if t == 'RIGHT' else 0) * mm, top2 + 6 * mm)
c.setFillColor(GREY); c.setFont('Cond', 7.5); c.drawString(18 * mm, 14 * mm, 'Photos: Peter Kohlmetz Moller. Official Teensy pinout card: pjrc.com/teensy/pinout.html')
c.showPage()

header('Real pictures · Ksoloti Core v0.6', 'From Ksoloti\'s official pin-function sheet (v0.6 and earlier)')
y = H - 42 * mm
from reportlab.lib.utils import ImageReader
ir = ImageReader('ks_full.png'); iw, ih = ir.getSize(); fh = 150 * mm; fw = fh * iw / ih
c.drawImage(ir, 18 * mm, y - fh, fw, fh)
rx = 18 * mm + fw + 6 * mm; rw = W - 18 * mm - rx
b = photo('ks_h3.png', rx, y, rw, 'H3, top rows: USB HOST D+ / D−, 5.0V, GND')
for t, fx, fy, col in (('D+', 0.30, 0.18, CDP), ('D−', 0.34, 0.18, CDM), ('5V', 0.30, 0.5, C5V), ('GND', 0.34, 0.82, CGND)):
    pass
text(rx, b - 10 * mm, ['**Half 2 → H3:**', 'red → 5.0V', 'white → USB HOST D−', 'green → USB HOST D+', 'black → GND'], size=8.6, lead=11.5)
b2 = photo('ks_j1.png', rx, b - 72 * mm, rw, 'J1 = LINE header (pin 1 is the square pad)')
text(rx, b2 - 10 * mm, ['**Audio from J1:**', 'LINE OUT LEFT CH → left jack tip', 'LINE OUT RIGHT CH → right jack tip',
    'LINE OUT GROUND → both sleeves'], size=8.6, lead=11.5)
c.setFillColor(GREY); c.setFont('Cond', 7.5)
c.drawString(18 * mm, 18 * mm, 'Pin sheet: Ksoloti project, ksoloti.github.io/docs/ksoloti_core_pin_functions_v06_and_earlier.pdf (v0.7+ boards: use the v0.7 sheet).')
c.showPage()

# ============================================================ page 5: jacks, mounting, test
header('Step 3 · Jacks, mounting and first test')
y0 = H - 92 * mm
# back panel drawing
c.setFillColor(CASE); c.roundRect(20 * mm, y0 - 22 * mm, 170 * mm, 44 * mm, 2 * mm, stroke=0, fill=1)
c.setFillColor(white); c.setFont('CondB', 8); c.drawString(24 * mm, y0 + 16 * mm, 'iC back panel (seen from behind)')
c.setFillColor(HexColor('#c9c9c9')); c.rect(98 * mm, y0 - 14 * mm, 14 * mm, 12 * mm, stroke=0, fill=1)
c.setFillColor(INK); c.setFont('CondB', 6.5); c.drawCentredString(105 * mm, y0 - 18 * mm, 'USB')
jack(97 * mm, y0 + 6 * mm, 'L', CL); jack(113 * mm, y0 + 6 * mm, 'R', CR)
c.setFillColor(white); c.setFont('Cond', 7.5); c.drawString(124 * mm, y0 + 5 * mm, '← two new holes above the USB socket')
c.drawString(124 * mm, y0 + 0 * mm, '   (drill to fit your jacks, check the inside first)')
box(18 * mm, 104 * mm, 84 * mm, 62 * mm, 'Mounting the board', [
    'Stick the Teensy (with shield) or the', '   Ksoloti flat on the iC main board',
    '   with strong double-sided nano tape.', 'Put Kapton or insulating tape under it',
    '   first: no pin may touch the main board.', 'Keep it clear of the case screws and',
    '   the keyboard, and keep wires short.', 'Teensy: SD card slot stays reachable.'])
box(108 * mm, 104 * mm, 84 * mm, 62 * mm, 'First test, before closing', [
    '1. Flash the engine (Teensy sketch or', '   Ksoloti patch) with the board outside',
    '   the iC first.', '2. Connect a computer to the iC\'s USB',
    '   socket: the board powers up and the iC', '   LEDs light.',
    '3. Play: sound from the new jacks.', '4. MIDI Monitor shows "Twenty Engine".'])
box(18 * mm, 30 * mm, 174 * mm, 64 * mm, 'If something doesn\'t work', [
    '**No sound, but the iC lights up:**  check L / R / G at the LINE OUT pads and the jack lugs; G must reach both sleeves.',
    '**iC doesn\'t light up:**  half 2 (host side) is wrong. Check 5V and GND first, then whether D+ and D− are swapped.',
    '**Computer doesn\'t see "Twenty Engine":**  half 1 (device side). Same check: power, ground, then D+ / D−.',
    'Swapped D+ / D− damages nothing; the device simply isn\'t found.',
    '**Hum or noise:**  keep audio wires away from the USB wires and the power section; twist L and G together.',
    'Questions: send a photo of your wiring along with what you see. That is the fastest way to find it.'])
c.setFillColor(GREY); c.setFont('Cond', 7.5)
c.drawString(18 * mm, 18 * mm, 'Twenty Engine by Peter Kohlmetz Moller. GPL v3. Teensy is a trademark of PJRC; Ksoloti is an open-hardware project by Ksoloti.')
c.drawString(18 * mm, 13.5 * mm, 'MS-20 iC is a product of Korg. This is an independent modification guide, not endorsed by Korg.')
c.showPage(); c.save()
