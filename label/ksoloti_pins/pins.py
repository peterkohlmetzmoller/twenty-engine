from PIL import Image, ImageDraw, ImageFont
from PIL import ImageEnhance
B = Image.open('board.png').convert('RGB')
B = ImageEnhance.Color(B).enhance(0.15)                       # almost grey: the pin rings stand out
B = Image.blend(B, Image.new('RGB', B.size, 'white'), 0.35)    # and lighter
F = '/usr/share/fonts/truetype/dejavu/'
fT = ImageFont.truetype(F + 'DejaVuSansCondensed-Bold.ttf', 46)
fH = ImageFont.truetype(F + 'DejaVuSansCondensed-Bold.ttf', 40)
fL = ImageFont.truetype(F + 'DejaVuSansCondensed.ttf', 36)
W, H = 2980, 2380
img = Image.new('RGB', (W, H), 'white'); d = ImageDraw.Draw(img)
ox, oy = 830, 300
img.paste(B, (ox, oy))
RED, WHITE, GREEN, BLACK = (216, 50, 42), (255, 255, 255), (47, 163, 90), (21, 21, 21)
BLUE, YEL, ORANGE, INK = (47, 111, 214), (232, 180, 32), (232, 84, 42), (21, 21, 21)
P = lambda x, y: (ox + x, oy + y)

def ring(x, y, col, r=24):
    cx, cy = P(x, y)
    d.ellipse((cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4), outline=INK, width=3)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=col, width=9)

def wire(pts, col):
    q = [P(*p) if i == 0 else p for i, p in enumerate(pts)]     # first point = pin (board coords), rest canvas
    d.line(q, fill=INK, width=17, joint='curve')
    d.line(q, fill=col, width=11, joint='curve')

def group(x0, y0, x1, y1, col):
    d.rounded_rectangle((*P(x0, y0), *P(x1, y1)), radius=14, outline=col, width=6)

fS = ImageFont.truetype(F + 'DejaVuSansCondensed-Oblique.ttf', 33)
def label(x, y, title, sub, rows, col):
    w = 720; top = 82 + (50 if sub else 0); h = top + len(rows) * 54 + 20
    d.rounded_rectangle((x, y, x + w, y + h), radius=18, fill=(244, 244, 242), outline=col, width=5)
    d.text((x + 28, y + 18), title, font=fH, fill=INK)
    if sub: d.text((x + 28, y + 72), sub, font=fS, fill=(90, 90, 90))
    for i, (name, c) in enumerate(rows):
        yy = y + top + i * 54
        d.ellipse((x + 30, yy + 4, x + 62, yy + 36), fill=c, outline=INK, width=3)
        d.text((x + 82, yy), name, font=fL, fill=INK)

d.text((60, 60), 'Ksoloti Core v0.6 · pins used by Twenty Engine', font=fT, fill=INK)

# USB-C sockets on the top edge
for (x, y, t) in ((700, 120, 'USB DEVICE (PROG)'), (980, 120, 'USB HOST')):
    cx, cy = P(x, y)
    d.rounded_rectangle((cx - 115, cy - 95, cx + 115, cy + 70), radius=22, outline=ORANGE, width=7)
d.text((P(700, 0)[0] - 330, oy - 120), 'USB DEVICE (PROG)', font=fH, fill=ORANGE)
d.text((P(980, 0)[0] - 40, oy - 120), 'USB HOST', font=fH, fill=ORANGE)

# H2: USB device pins  <- incoming USB-B half
dev = [(795, 438, GREEN), (795, 488, WHITE), (845, 488, BLACK), (895, 488, RED)]
# up, then left; heights staggered so no wire crosses another wire or a pin
cx = lambda x: ox + x; cy = lambda y: oy + y
wire([(795, 488), (cx(765), cy(488)), (cx(765), 658), (700, 658)], WHITE)   # D-: step left, then up
wire([(795, 438), (cx(795), 640), (700, 640)], GREEN)                         # D+
wire([(845, 488), (cx(845), 622), (700, 622)], BLACK)                         # GND
wire([(895, 488), (cx(895), 604), (700, 604)], RED)                           # 5V
group(765, 408, 925, 518, BLUE)
for x, y, c in dev: ring(x, y, c)
label(-40 + 60, 440, 'USB DEVICE · H2', 'Connect the incoming USB-B half here',
      [('USB PROG D+', GREEN), ('USB PROG D−', WHITE), ('GND', BLACK), ('5V', RED)], BLUE)

# H3: USB host pins  <- MS-20ic main board half
host = [(1045, 438, GREEN), (1095, 438, WHITE), (1045, 485, RED), (1045, 537, BLACK)]
wire([(1095, 438), (cx(1095), 658), (2200, 658)], WHITE)                      # D-
wire([(1045, 438), (cx(1045), 640), (2200, 640)], GREEN)                      # D+
wire([(1045, 485), (cx(1015), cy(485)), (cx(1015), 622), (2200, 622)], RED)   # 5V: step left, then up
wire([(1045, 537), (cx(998), cy(537)), (cx(998), 604), (2200, 604)], BLACK)   # GND
group(1015, 408, 1125, 567, BLUE)
for x, y, c in host: ring(x, y, c)
label(2200, 480, 'USB HOST · H3', 'Connect the MS-20ic main board half here',
      [('USB HOST D+', GREEN), ('USB HOST D−', WHITE), ('5V', RED), ('GND', BLACK)], BLUE)

# J1: line out
aud = [(240, 1792, YEL), (290, 1792, BLACK), (340, 1792, BLUE)]
for i, (x, y, c) in enumerate(aud):
    yy = 2186 - i * 18
    wire([(x, y), (ox + x, yy), (700, yy)], c)
group(210, 1762, 370, 1822, (120, 70, 160))
for x, y, c in aud: ring(x, y, c)
label(60, 1960, 'LINE OUT · J1', None, [('LINE OUT RIGHT CH', YEL), ('LINE OUT GROUND', BLACK), ('LINE OUT LEFT CH', BLUE)], (120, 70, 160))

d.text((60, H - 60), 'Board image: Ksoloti project (ksoloti.github.io)', font=ImageFont.truetype(F + 'DejaVuSansCondensed.ttf', 28), fill=(120, 120, 120))
img.save('/mnt/user-data/outputs/Ksoloti_TwentyEngine_pins.png')
img.resize((W // 3, H // 3)).save('preview.png')
