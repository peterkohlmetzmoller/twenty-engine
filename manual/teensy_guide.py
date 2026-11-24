# teensy_guide.py - Teensy edition: key click and buttons (same look as the manual)
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

OUT = sys.argv[1] if len(sys.argv) > 1 else 'TwentyEngine_Teensy_KeyClick_Buttons.pdf'
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
...
static const Button BUTTONS[] = {
  // { 2, FUNCTION },
  // { 3, NEXT },
  { -1, NEXT }                         // (keep this last line: it is not a button)
};'''

EXAMPLE = '''static const float KEY_CLICK = 0.0f;   // no click
static const Button BUTTONS[] = {
  { 2, FUNCTION },
  { 3, PREV },
  { 4, NEXT },
  { 5, RETRIG },
  { -1, NEXT }                         // (keep this last line: it is not a button)
};'''

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

sw = [Paragraph('Buttons', h1),
      Paragraph('Push buttons on free Teensy pins, as many as you like, each with its own function. Give each button a line in '
                '<b>BUTTONS</b>: its pin and what it does. Remove the <b>//</b> in front of a line to use it.', body),
      table([['<b>Function</b>', '<b>What the button does</b>'],
             ['<b>FUNCTION</b>', 'A function switch of its own. Hold it + tap a key: recall a sound; hold it + hold a key 2 s: save; '
                                 'hold it: the ESP knobs\' delay layer; with the tap cable (SWITCH → ESP SIGNAL IN) its taps set the tempo. '
                                 'The iC\'s momentary switch then only does what you patch SWITCH to.'],
             ['<b>NEXT</b>', 'Next sound (as switch + the next memory key). From the knobs it goes to sound 1, after 36 back to 1.'],
             ['<b>PREV</b>', 'Previous sound. From the knobs it goes to sound 36.'],
             ['<b>PANEL</b>', 'Back to the knobs (as switch + top C).'],
             ['<b>MONO</b>', 'Mono on / off (as plugging or pulling KBD CV OUT → VCO1+2 CV IN).'],
             ['<b>ESP_LAYER</b>', 'The ESP knobs on the delay layer, and they stay there until you press it again (as holding the switch, '
                                  'but the keys keep playing).'],
             ['<b>TAP</b>', 'Tap tempo without the tap cable: two taps set the delay time; with the arp on, taps are quarter notes.'],
             ['<b>RETRIG</b>', 'Retrigger on / off: in mono every new key starts the envelopes again, also when you play legato '
                               '(the MS-20\'s multiple trigger). TRIG blinks once: on. PEAK blinks once: off.']],
            [26 * mm, W - 2 * M - 26 * mm]),
      Spacer(1, 2 * mm),
      Paragraph('Wiring', h2),
      Paragraph('Use push buttons (momentary), not on/off switches. One leg of each button to its pin, the other leg to <b>GND</b>. No resistor needed: the Teensy\'s own pull-up is on. '
                'Free pins with the audio board: <b>0-5, 9, 14, 16, 17, 22, 24-41</b>. The audio board uses 7, 8, 18-21 and 23.', body),
      Paragraph('Example', h2),
      Paragraph('A function switch on pin 2, previous / next sound on 3 and 4, retrigger on 5, and no key click:', body),
      box(Preformatted(EXAMPLE, code)), Spacer(1, 3 * mm),
      Paragraph('Good to know', h2),
      Paragraph('• All LEDs blink once when a sound is recalled, as with the keys.<br/>'
                '• With a cable from SWITCH to ESP SIGNAL IN (tap tempo), the memory is off, so NEXT, PREV and PANEL do nothing. '
                'Same as on the panel.<br/>'
                '• The real mono cable still works too. The MONO button always switches to the other state.<br/>'
                '• The 1.0.x settings SWITCH_NEXT, _PREV, _PANEL and _MONO (further down in SETTINGS) still work.', body)]
story += sw



def deco(c, doc):
    c.saveState()
    if doc.page == 1:
        c.setFont('Cond', 22); c.setFillColor(INK); c.drawString(M, H - M - 8 * mm, 'Key click and buttons')
        c.setStrokeColor(RED); c.setLineWidth(0.8); c.line(M, H - M - 10.5 * mm, M + 120 * mm, H - M - 10.5 * mm)
        c.setFillColor(RED); c.rect(M, H - M - 12 * mm, 3 * mm, 3 * mm, fill=1, stroke=0)
    c.setFont('CondB', 9); c.setFillColor(GREY); c.drawRightString(W - M, H - M + 2 * mm, 'TWENTY ENGINE  ·  TEENSY 4.1')
    c.setFont('Cond', 8.5); c.drawRightString(W - M, M - 8 * mm, str(doc.page))
    c.setFont('Cond', 7.2); c.drawString(M, M - 8 * mm, 'Twenty Engine is free software (GPL v3): you are welcome to change it. '
                                         'MS-20 is a trademark of KORG Inc.; Twenty Engine is an independent project.')
    c.restoreState()


doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=M, bottomMargin=M,
                      title='Twenty Engine - Teensy edition: key click and buttons', author='Peter Kohlmetz Moller')
first = Frame(M, M, W - 2 * M, H - 2 * M - 16 * mm, 0, 0, 0, 0, id='f1')
later = Frame(M, M, W - 2 * M, H - 2 * M, 0, 0, 0, 0, id='f2')
doc.addPageTemplates([PageTemplate('first', [first], onPage=deco, autoNextPageTemplate='later'),
                      PageTemplate('later', [later], onPage=deco)])
doc.build(story)
print(OUT)
