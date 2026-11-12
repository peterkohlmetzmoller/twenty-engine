# loopmidi_guide.py - one page for Windows: keep each take's sound with loopMIDI (same look as the manual)
# python3 manual/loopmidi_guide.py out.pdf
import sys
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import registerFontFamily
D = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Cond', D + 'DejaVuSansCondensed.ttf'))
pdfmetrics.registerFont(TTFont('CondB', D + 'DejaVuSansCondensed-Bold.ttf'))
pdfmetrics.registerFont(TTFont('CondI', D + 'DejaVuSansCondensed-Oblique.ttf'))
pdfmetrics.registerFont(TTFont('CondBI', D + 'DejaVuSansCondensed-BoldOblique.ttf'))
registerFontFamily('Cond', normal='Cond', bold='CondB', italic='CondI', boldItalic='CondBI')
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph, Frame, KeepInFrame

OUT = sys.argv[1] if len(sys.argv) > 1 else 'TwentyEngine_Windows_loopMIDI_Guide.pdf'
PAGE = landscape(A4); W, H = PAGE; M = 16 * mm
RED = HexColor('#d8322a'); INK = HexColor('#151515'); GREY = HexColor('#777777'); LIGHT = HexColor('#f2f2f0')
LINK = 'https://www.tobias-erichsen.de/software/loopmidi.html'

body = ParagraphStyle('b', fontName='Cond', fontSize=10.2, leading=14, textColor=INK, spaceAfter=4)
small = ParagraphStyle('s', parent=body, fontSize=8.6, leading=11.4, textColor=GREY)
head = ParagraphStyle('h', fontName='CondB', fontSize=11.5, leading=14, textColor=INK, spaceAfter=3)

STEPS = [
    ('Get loopMIDI', f'Free, by Tobias Erichsen: <a href="{LINK}" color="#d8322a"><u>tobias-erichsen.de/software/loopmidi.html</u></a>. '
                     'Download, install and start it. (The Twenty Engine installer can open this page for you at the end.)'),
    ('Make the port', 'In loopMIDI, type <b>Twenty Engine Panel</b> in the box at the bottom ("New port-name"), exactly like that, '
                      'and click <b>+</b>. Keep loopMIDI running whenever you make music: it sits in the taskbar.'),
    ('Turn it on in your DAW', '<b>REAPER:</b> Options &gt; Preferences &gt; Audio &gt; MIDI Inputs: enable <b>Twenty Engine Panel</b> '
                               '(and the MS-20 Controller). <b>Ableton Live:</b> Settings &gt; Link, Tempo &amp; MIDI: Track On for both. '
                               '<b>Cubase:</b> Studio &gt; Studio Setup &gt; MIDI Port Setup: tick <i>In \'All MIDI Inputs\'</i> for both.'),
    ('Set the track input', 'On the Twenty Engine track, set the MIDI input to <b>all MIDI inputs</b>, all channels '
                            '(REAPER: Input: MIDI &gt; All MIDI inputs. Ableton: MIDI From: All Ins. Cubase: input routing '
                            'All MIDI Inputs). Arm the track as usual.'),
    ('Play and record', 'That\'s it. Record as you always do: each take now keeps its starting sound and every knob move, also when '
                        'you move, cut or copy it. When a take is not playing, the sound is the panel\'s again.'),
]


def draw(c):
    c.setFillColor(white); c.rect(0, 0, W, H, fill=1, stroke=0)
    # title, as in the manual
    c.setFont('Cond', 22); c.setFillColor(INK); c.drawString(M, H - M - 8 * mm, 'Windows: keep each take\'s sound')
    c.setStrokeColor(RED); c.setLineWidth(0.8); c.line(M, H - M - 10.5 * mm, M + 120 * mm, H - M - 10.5 * mm)
    c.setFillColor(RED); c.rect(M, H - M - 12 * mm, 3 * mm, 3 * mm, fill=1, stroke=0)
    c.setFont('CondB', 10); c.setFillColor(GREY); c.drawRightString(W - M, H - M - 8 * mm, 'TWENTY ENGINE  ·  1.0.2')
    intro = Paragraph('On a Mac the plugin keeps every take\'s sound by itself. Windows 10 does not let a plugin make a MIDI port of '
                      'its own, so it uses one made with the free <b>loopMIDI</b>. Setup takes two minutes and is done once. '
                      'Without it, Twenty Engine plays the panel\'s sound like any synth.', body)
    f = Frame(M, H - M - 36 * mm, W - 2 * M, 22 * mm, 0, 0, 0, 0, showBoundary=0); f.addFromList([intro], c)
    # five steps in a row of cards
    gap = 5 * mm; cw = (W - 2 * M - 4 * gap) / 5; top = H - M - 42 * mm; ch = 104 * mm
    for i, (t, txt) in enumerate(STEPS):
        x = M + i * (cw + gap)
        c.setFillColor(LIGHT); c.rect(x, top - ch, cw, ch, fill=1, stroke=0)
        c.setFillColor(RED); c.rect(x, top - 11 * mm, 11 * mm, 11 * mm, fill=1, stroke=0)
        c.setFillColor(white); c.setFont('CondB', 15); c.drawCentredString(x + 5.5 * mm, top - 8.2 * mm, str(i + 1))
        story = [Paragraph(t, head), Paragraph(txt, body)]
        fr = Frame(x + 4 * mm, top - ch + 4 * mm, cw - 8 * mm, ch - 19 * mm, 0, 0, 0, 0, showBoundary=0)
        fr.addFromList([KeepInFrame(cw - 8 * mm, ch - 19 * mm, story, mode='shrink')], c)
        c.linkURL(LINK, (x, top - ch, x + cw, top), relative=0) if i == 0 else None
    # check and troubleshooting
    y = top - ch - 8 * mm
    tips = [
        '<b>Check:</b> the plugin\'s window says "The sound is recorded with the regions" once it has found the port.',
        '<b>Takes recorded before the setup</b> play with the panel\'s sound, as before. Takes recorded from now on keep theirs.',
        '<b>Several Twenty Engine tracks:</b> the track you last played on is the one that records its sound.',
        '<b>Nothing changed?</b> Check the port name (Twenty Engine Panel, with spaces), that loopMIDI is running, and that the track input is all MIDI inputs.',
    ]
    fr = Frame(M, M - 2 * mm, W - 2 * M, y - M + 2 * mm, 0, 0, 0, 0, showBoundary=0)
    fr.addFromList([Paragraph(t, body) for t in tips] + [Paragraph('loopMIDI is free software by Tobias Erichsen, not part of Twenty Engine. '
                   'MS-20 is a trademark of KORG Inc.; Twenty Engine is an independent project.', small)], c)


from reportlab.pdfgen import canvas
c = canvas.Canvas(OUT, pagesize=PAGE)
c.setTitle('Twenty Engine - Windows: keep each take\'s sound (loopMIDI)'); c.setAuthor('Peter Kohlmetz Moller')
draw(c); c.showPage(); c.save()
print(OUT)
