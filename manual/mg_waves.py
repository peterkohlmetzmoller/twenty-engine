# MG waveform morph, drawn from the engine's own LFO formulas (O_MG_Morphing_LFO in engine_gen.h).
import math
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white

INK = HexColor('#151515'); OR = HexColor('#d9481f'); LIGHT = HexColor('#f2f2f0'); GREY = HexColor('#777777')

def simulate(shape, cycles=2.0, per=320, seed=987654321):
    inc = int(2 ** 32 / per) & 0xFFFFFFFF
    ph = phf = 0; rng = seed; r_t = r_s1 = r_s2 = 0.0
    slw = min(0.25, 0.004 + ((inc >> 14) * 0.00015))
    out = []
    warm = per * 3                                   # let the random settle before we draw
    for n in range(warm + int(cycles * per)):
        op = ph; ph = (ph + inc) & 0xFFFFFFFF; phf = (phf + inc * 3 + (inc >> 1)) & 0xFFFFFFFF
        xs = (ph - 2 ** 32 if ph >= 2 ** 31 else ph) * 4.6566e-10
        sw = 4 * xs * (1 - abs(xs))
        sq = max(-1.0, min(1.0, sw * 12))
        rng = (rng * 196314165 + 907633515) & 0xFFFFFFFF
        if ph < op or (rng & 0x7F) == 0: r_t = (rng - 2 ** 32 if rng >= 2 ** 31 else rng) * 4.6566e-10
        r_s1 += slw * (r_t - r_s1); r_s2 += slw * (r_s1 - r_s2)
        xf = (phf - 2 ** 32 if phf >= 2 ** 31 else phf) * 4.6566e-10
        fs = 4 * xf * (1 - abs(xf))
        wf = max(-1.0, min(1.0, r_s2 * 1.1 + 0.25 * fs * r_s1))
        if shape <= 0.5: m = shape * 2; v = sw * (1 - m) + sq * m
        else: m = (shape - 0.5) * 2; v = sq * (1 - m) + wf * m
        if n >= warm: out.append(v)
    return out

def mini_knob(c, x, y, r, f):
    """small knob icon, 300 degree travel like the panel drawings"""
    c.setStrokeColor(INK); c.setLineWidth(0.3)
    for i in range(11):
        a = math.radians(240 - 30 * i)
        c.line(x + r * 1.18 * math.cos(a), y + r * 1.18 * math.sin(a), x + r * 1.45 * math.cos(a), y + r * 1.45 * math.sin(a))
    c.setFillColor(white); c.setLineWidth(0.5); c.circle(x, y, r, stroke=1, fill=1)
    a = math.radians(240 - 300 * f)
    c.setLineWidth(r * 0.16); c.setLineCap(1)
    c.line(x + r * 0.1 * math.cos(a), y + r * 0.1 * math.sin(a), x + r * 0.85 * math.cos(a), y + r * 0.85 * math.sin(a)); c.setLineCap(0)

def draw(c, w, h, font='Cond', fontb='CondB'):
    steps = [(0.0, 'TRIANGLE', 'rounded, close to a sine'), (0.25, '', ''), (0.5, 'SQUARE', 'soft edges'), (0.75, '', ''), (1.0, 'SMOOTH RANDOM', 'wandering, never repeats')]
    n = len(steps); gap = 3 * mm; bw = (w - gap * (n - 1)) / n
    kr = 2.6 * mm; ky = h - kr * 1.6
    by1 = ky - kr * 1.6 - 2 * mm; by0 = 11 * mm; bh = by1 - by0
    # arrow under the knobs: the direction of the morph
    c.setStrokeColor(GREY); c.setLineWidth(0.5)
    for i, (f, name, sub) in enumerate(steps):
        x0 = i * (bw + gap); xc = x0 + bw / 2
        mini_knob(c, xc, ky, kr, f)
        c.setFillColor(LIGHT); c.roundRect(x0, by0, bw, bh, 2 * mm, stroke=0, fill=1)
        c.setStrokeColor(HexColor('#d0d0cc')); c.setLineWidth(0.4); c.line(x0 + 2 * mm, by0 + bh / 2, x0 + bw - 2 * mm, by0 + bh / 2)
        v = simulate(f, per=320, seed=6228)
        px0 = x0 + 3 * mm; pw = bw - 6 * mm; amp = bh * 0.36
        p = c.beginPath()
        for k, s in enumerate(v):
            X = px0 + pw * k / (len(v) - 1); Y = by0 + bh / 2 + s * amp
            p.moveTo(X, Y) if k == 0 else p.lineTo(X, Y)
        col = OR if f > 0.5 else INK
        c.setStrokeColor(col); c.setLineWidth(1.1); c.setLineJoin(1); c.drawPath(p, stroke=1, fill=0)
        if name:
            c.setFillColor(INK); c.setFont(fontb, 8.2); c.drawCentredString(xc, by0 - 4.2 * mm, name)
            c.setFillColor(GREY); c.setFont(font, 7.2); c.drawCentredString(xc, by0 - 7.8 * mm, sub)
        if i < n - 1:                                   # arrow between boxes
            ax = x0 + bw + gap / 2; ay = by0 + bh / 2
            c.setFillColor(GREY); p2 = c.beginPath(); p2.moveTo(ax - 1.0 * mm, ay + 1.3 * mm); p2.lineTo(ax + 1.0 * mm, ay); p2.lineTo(ax - 1.0 * mm, ay - 1.3 * mm); p2.close(); c.drawPath(p2, stroke=0, fill=1)
