from PIL import Image, ImageDraw, ImageFont, ImageEnhance
def soft(im):
    im = ImageEnhance.Color(im.convert('RGB')).enhance(0.15)
    return Image.blend(im, Image.new('RGB', im.size, 'white'), 0.35)
F = '/usr/share/fonts/truetype/dejavu/'
fT = ImageFont.truetype(F + 'DejaVuSansCondensed-Bold.ttf', 46)
fH = ImageFont.truetype(F + 'DejaVuSansCondensed-Bold.ttf', 40)
fL = ImageFont.truetype(F + 'DejaVuSansCondensed.ttf', 36)
fS = ImageFont.truetype(F + 'DejaVuSansCondensed-Oblique.ttf', 33)
RED, WHITE, GREEN, BLACK = (216, 50, 42), (255, 255, 255), (47, 163, 90), (21, 21, 21)
BLUE, YEL, ORANGE, INK, PURP = (47, 111, 214), (232, 180, 32), (232, 84, 42), (21, 21, 21), (120, 70, 160)
W, H = 2980, 2420
img = Image.new('RGB', (W, H), 'white'); d = ImageDraw.Draw(img)
d.text((60, 60), 'Teensy 4.1 + Audio Shield Rev D · pins used by Twenty Engine', font=fT, fill=INK)

# boards
TS = 1.1; tox, toy = 980, 230
T = soft(Image.open('teensy.png')); T = T.resize((int(T.size[0] * TS), int(T.size[1] * TS)), Image.LANCZOS)
img.paste(T, (tox, toy))
SS = 3.4; sox, soy = 1760, 1180
S = soft(Image.open('shield.png')); S = S.resize((int(S.size[0] * SS), int(S.size[1] * SS)), Image.LANCZOS)
img.paste(S, (sox, soy))
PT = lambda x, y: (tox + x * TS, toy + y * TS)
PS = lambda x, y: (sox + x * SS, soy + y * SS)

def ring(c, col, r=28):
    cx, cy = c
    d.ellipse((cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4), outline=INK, width=3)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=col, width=9)
def wire(pts, col):
    d.line(pts, fill=INK, width=17, joint='curve'); d.line(pts, fill=col, width=11, joint='curve')
def label(x, y, title, sub, rows, col):
    w = 760; top = 82 + (50 if sub else 0); h = top + len(rows) * 54 + 20
    d.rounded_rectangle((x, y, x + w, y + h), radius=18, fill=(244, 244, 242), outline=col, width=5)
    d.text((x + 28, y + 18), title, font=fH, fill=INK)
    if sub: d.text((x + 28, y + 72), sub, font=fS, fill=(90, 90, 90))
    for i, (name, c) in enumerate(rows):
        yy = y + top + i * 54
        d.ellipse((x + 30, yy + 4, x + 62, yy + 36), fill=c, outline=INK, width=3)
        d.text((x + 82, yy), name, font=fL, fill=INK)
    return x + w

# USB DEVICE: micro-USB socket
ux, uy = PT(285, 30)
for i, c in enumerate([RED, WHITE, GREEN, BLACK]):
    xx = ux - 45 + i * 30; yy = 170 + i * 0 
    wire([(xx, uy), (xx, 190 + i * 18), (1760, 190 + i * 18)], c)
a, b = PT(165, 5), PT(405, 190)
d.rounded_rectangle((a[0], a[1], b[0], b[1]), radius=22, outline=ORANGE, width=7)
label(1760, 140, 'USB DEVICE · micro-USB', 'Micro-USB cable from the USB-B board',
      [('5V', RED), ('D−', WHITE), ('D+', GREEN), ('GND', BLACK)], BLUE)

# USB HOST: 5 pads beside pins 2-6 (from the top: 5V, D-, D+, GND, GND)
host = [(283, RED), (360, WHITE), (440, GREEN), (518, BLACK)]
for y, c in host:
    cx, cy = PT(142, y)
    wire([(cx, cy), (820, cy)], c)
a, b = PT(108, 245), PT(178, 635)
d.rounded_rectangle((a[0], a[1], b[0], b[1]), radius=14, outline=BLUE, width=6)
for y, c in host: ring(PT(142, y), c)
ring(PT(142, 597), BLACK)
label(60, 450, 'USB HOST · 5 pads', 'Connect the MS-20ic main board half here',
      [('5V', RED), ('D−', WHITE), ('D+', GREEN), ('GND (either pad)', BLACK)], BLUE)

# LINE OUT on the audio shield (underside): L G R
aud = [(121.7, 46.7, BLUE), (145, 46.7, BLACK), (166.7, 46.7, YEL)]
for i, (x, y, c) in enumerate(aud):
    cx, cy = PS(x, y)
    wire([(cx, cy), (cx, 960 + i * 18), (1760 + 120, 960 + i * 18)], c) if False else None
lx = 1760; ly = 690
for i, (x, y, c) in enumerate(aud):
    cx, cy = PS(x, y)
    wire([(cx, cy), (cx, ly + 314)], c)
label(lx + 300, ly, 'LINE OUT · audio shield', 'Underside of the shield',
      [('L  → left jack tip', BLUE), ('G  → both jack sleeves', BLACK), ('R  → right jack tip', YEL)], PURP)
for i, (x, y, c) in enumerate(aud):
    cx, cy = PS(x, y)
    wire([(cx, 1080 - i * 18), (lx + 300 + 380 + (i - 1) * 0, 1080 - i * 18)], c) if False else None
a, b = PS(108, 34), PS(180, 60)
d.rounded_rectangle((a[0], a[1], b[0], b[1]), radius=14, outline=PURP, width=6)
for x, y, c in aud: ring(PS(x, y), c, r=26)
d.text((60, H - 60), 'Board images: PJRC (pjrc.com)', font=ImageFont.truetype(F + 'DejaVuSansCondensed.ttf', 28), fill=(120, 120, 120))
img.save('/mnt/user-data/outputs/Teensy_TwentyEngine_pins.png')
img.resize((W // 3, H // 3)).save('preview.png')
