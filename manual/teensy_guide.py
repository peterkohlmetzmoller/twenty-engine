# teensy_guide.py - Teensy edition: key click and extra switches (same look as the manual)
# python3 manual/teensy_guide.py out.pdf
import sys
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import registerFontFamily
D = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Cond', D + 'DejaVuSansCondensed.ttf'))
pdfmetrics.registerFont(TTFont('CondB', D + 'DejaVuSansCondensed-Bold.ttf'))
pdfmetrics.registerFont(TTFont('CondI', D + 'DejaVuSansCondensed-Oblique.ttf'))
pdfmetrics.registerFont(TTFont('CondBI', D + 'DejaVuSansCondensed-BoldOblique.ttf'))
pdfmetrics.registerFont(TTFont('Mono', D + 'DejaVuSansMono.ttf'))
registerFontFamily('Cond', normal='Cond', bold='CondB', italic='CondI', boldItalic='CondBI')
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                Preformatted, KeepTogether)

OUT = sys.argv[1] if len(sys.argv) > 1 else 'TwentyEngine_Teensy_KeyClick_Switches.pdf'
W, H = A4; M = 18 * mm
RED = HexColor('#d8322a'); INK = HexColor('#151515'); GREY = HexColor('#777777'); LIGHT = HexColor('#f2f2f0')
REPO = 'https://github.com/peterkohlmetzmoller/twenty-engine'
TD = 'https://www.pjrc.com/teensy/td_download.html'

body = ParagraphStyle('b', fontName='Cond', fontSize=10.2, leading=14, textColor=INK, spaceAfter=5)
small = ParagraphStyle('s', parent=body, fontSize=8.6, leading=11.4, textColor=GREY)
h1 = ParagraphStyle('h1', fontName='Cond', fontSize=17, leading=21, textColor=INK, spaceBefore=8, spaceAfter=6)
h2 = ParagraphStyle('h2', fontName='CondB', fontSize=11.5, leading=14, textColor=INK, spaceBefore=4, spaceAfter=3)
cell = ParagraphStyle('c', parent=body, spaceAfter=0)
code = ParagraphStyle('code', fontName='Mono', fontSize=8.4, leading=11.2, textColor=INK)


def link(url, text=None):
    return f'<a href="{url}" color="#d8322a"><u>{text or url.replace("https://", "")}</u></a>'


def box(flow, pad=4 * mm):
    t = Table([[flow]], colWidths=[W - 2 * M])
    t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, -1), LIGHT), ('LEFTPADDING', (0, 0), (-1, -1), pad),
                           ('RIGHTPADDING', (0, 0), (-1, -1), pad), ('TOPPADDING', (0, 0), (-1, -1), pad),
                           ('BOTTOMPADDING', (0, 0), (-1, -1), pad)]))
    return t


def steps(rows):
    data = [[Paragraph(f'<font color="white"><b>{i + 1}</b></font>', ParagraphStyle('n', parent=cell, alignment=1, fontSize=12)),
             Paragraph(f'<b>{t}</b><br/>{x}', cell)] for i, (t, x) in enumerate(rows)]
    t = Table(data, colWidths=[9 * mm, W - 2 * M - 9 * mm])
    st = [('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (1, 0), (1, -1), 4 * mm), ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
          ('TOPPADDING', (0, 0), (-1, -1), 4)]
    for i in range(len(rows)):
        st.append(('BACKGROUND', (0, i), (0, i), RED))
    t.setStyle(TableStyle(st))
    return t


def table(rows, widths):
    t = Table([[Paragraph(a, cell) for a in r] for r in rows], colWidths=widths)
    t.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LINEBELOW', (0, 0), (-1, -2), 0.4, HexColor('#d0d0cc')),
                           ('BACKGROUND', (0, 0), (-1, 0), LIGHT), ('TOPPADDING', (0, 0), (-1, -1), 4),
                           ('BOTTOMPADDING', (0, 0), (-1, -1), 4)]))
    return t


SETTINGS = '''// ================= SETTINGS =================
...
static const float KEY_CLICK = 3.0f;   // 0 = off, 3 = standard, 64 = max

static const int SWITCH_NEXT  = -1;    // next sound
static const int SWITCH_PREV  = -1;    // previous sound
static const int SWITCH_PANEL = -1;    // back to the knobs
static const int SWITCH_MONO  = -1;    // mono on / off
// ============================================'''

EXAMPLE = '''static const float KEY_CLICK = 0.0f;   // no click

static const int SWITCH_NEXT  = 2;
static const int SWITCH_PREV  = 3;
static const int SWITCH_PANEL = 4;
static const int SWITCH_MONO  = 5;'''

story = []
story += [Paragraph('Key click', h1),
          Paragraph('The soft thump at the start of every note is the key click. It is there on purpose: the original MS-20 has it too. '
                    'You can make it quieter or turn it off.', body),
          table([['', '<b>How</b>', '<b>Values</b>'],
                 ['<b>Plugin</b>', 'In your DAW, open the plugin\'s parameters and find <b>Key Click</b> (under Filters). '
                                   'It is saved with your project.', '0 = off<br/>3 = standard<br/>64 = maximum'],
                 ['<b>Teensy</b>', '<b>KEY_CLICK</b> at the top of the sketch, then upload (below).', 'Same scale as the plugin']],
                [22 * mm, W - 2 * M - 22 * mm - 36 * mm, 36 * mm]),
          Spacer(1, 4 * mm)]

story += [Paragraph('Changing the Teensy settings', h1),
          Paragraph('Everything you can change is in one block at the top of the sketch. Nothing else in the code needs touching.', body),
          box(Preformatted(SETTINGS, code)), Spacer(1, 4 * mm),
          steps([
              ('Install the Arduino IDE and Teensyduino',
               f'Arduino IDE 2 from arduino.cc, then add Teensy support as described on {link(TD, "pjrc.com/teensy/td_download.html")}.'),
              ('Get the code',
               f'{link(REPO, "github.com/peterkohlmetzmoller/twenty-engine")}: <b>Code &gt; Download ZIP</b>, unzip, and open '
               '<b>teensy/TwentyEngineTeensy/TwentyEngineTeensy.ino</b>.'),
              ('Pick the board',
               '<b>Tools &gt; Board &gt; Teensy 4.1</b>, <b>Tools &gt; USB Type &gt; Serial + MIDI</b>, <b>Tools &gt; CPU Speed &gt; 600 MHz</b>.'),
              ('Change the settings', 'Edit the values in the SETTINGS block and save.'),
              ('Upload', 'Connect the Teensy\'s own USB port to the computer and click <b>Upload</b>. '
                         'If the Teensy Loader asks, press the small button on the Teensy.'),
          ]),
          Spacer(1, 2 * mm),
          Paragraph('<b>Your saved sounds</b> live in the Teensy\'s flash. To be safe before uploading: put a FAT32 SD card in the '
                    'Teensy\'s slot and save one sound (hold the switch, hold a key for 2 seconds). That also writes a backup, '
                    '<b>MS20MEM.BIN</b>, to the card. If the flash is ever empty, the Teensy loads the card\'s copy by itself.', body),
          Paragraph('<b>Latency:</b> our ready-made .hex uses 16-sample audio blocks. A standard Teensyduino build uses 128, which adds '
                    'about 2 ms. Everything else is the same. To match ours, set AUDIO_BLOCK_SAMPLES to 16 in AudioStream.h '
                    'of the Teensy 4 core.', small)]

sw = [Paragraph('Extra switches', h1),
      Paragraph('Up to four push buttons on free Teensy pins. Each one sends the engine the same message the MS-20ic would, '
                'so it does exactly what the panel does.', body),
      table([['<b>Setting</b>', '<b>Does the same as</b>'],
             ['<b>SWITCH_NEXT</b>', 'Hold the switch + tap the next memory key. From the knobs it goes to sound 1, after 36 back to 1.'],
             ['<b>SWITCH_PREV</b>', 'Hold the switch + tap the previous memory key. From the knobs it goes to sound 36.'],
             ['<b>SWITCH_PANEL</b>', 'Hold the switch + tap top C: back to the knobs.'],
             ['<b>SWITCH_MONO</b>', 'Plugging / pulling the cable KBD CV OUT → VCO1+2 CV IN: mono on / off.']],
            [38 * mm, W - 2 * M - 38 * mm]),
      Spacer(1, 4 * mm),
      Paragraph('Wiring', h2),
      Paragraph('One leg of each button to its pin, the other leg to <b>GND</b>. No resistor needed: the Teensy\'s own pull-up is on. '
                'Free pins with the audio board: <b>0-5, 9, 14, 16, 17, 22, 24-41</b>. The audio board uses 7, 8, 18-21 and 23.', body),
      Paragraph('Example', h2),
      Paragraph('Buttons on pins 2 to 5, and no key click:', body),
      box(Preformatted(EXAMPLE, code)), Spacer(1, 3 * mm),
      Paragraph('Good to know', h2),
      Paragraph('• All LEDs blink once when a sound is recalled, as with the keys.<br/>'
                '• With a cable from SWITCH to ESP SIGNAL IN (tap tempo), the memory is off, so NEXT, PREV and PANEL do nothing. '
                'Same as on the panel.<br/>'
                '• The real mono cable still works too. The MONO button always switches to the other state.<br/>'
                '• Use push buttons (momentary). On/off toggle switches would need a flip down and up per step.', body)]
story += [KeepTogether(sw)]
story += [Spacer(1, 6 * mm),
          Paragraph('Twenty Engine is free software (GPL v3): you are welcome to change it. MS-20 is a trademark of KORG Inc.; '
                    'Twenty Engine is an independent project.', small)]


def deco(c, doc):
    c.saveState()
    if doc.page == 1:
        c.setFont('Cond', 22); c.setFillColor(INK); c.drawString(M, H - M - 8 * mm, 'Key click and extra switches')
        c.setStrokeColor(RED); c.setLineWidth(0.8); c.line(M, H - M - 10.5 * mm, M + 120 * mm, H - M - 10.5 * mm)
        c.setFillColor(RED); c.rect(M, H - M - 12 * mm, 3 * mm, 3 * mm, fill=1, stroke=0)
    c.setFont('CondB', 9); c.setFillColor(GREY); c.drawRightString(W - M, H - M + 2 * mm, 'TWENTY ENGINE  ·  TEENSY 4.1')
    c.setFont('Cond', 8.5); c.drawRightString(W - M, M - 8 * mm, str(doc.page))
    c.restoreState()


doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=M, bottomMargin=M,
                      title='Twenty Engine - Teensy edition: key click and switches', author='Peter Kohlmetz Moller')
first = Frame(M, M, W - 2 * M, H - 2 * M - 16 * mm, 0, 0, 0, 0, id='f1')
later = Frame(M, M, W - 2 * M, H - 2 * M, 0, 0, 0, 0, id='f2')
doc.addPageTemplates([PageTemplate('first', [first], onPage=deco, autoNextPageTemplate='later'),
                      PageTemplate('later', [later], onPage=deco)])
doc.build(story)
print(OUT)
