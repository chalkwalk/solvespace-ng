#!/usr/bin/env python3
# Writes the NG tool shelf icons (24x24 SVG). Run from this directory: python3 generate.py
#
# The icons mirror the drawing area (decided 2026-10-17 with the owner, after comparing
# SolveSpace's own toolbar, FreeCAD, Onshape and Fusion 360; docs/research/cad-icon-conventions.md):
# sketch entities are lines with square points, constraints are the marks the canvas draws, in
# the canvas colours; features are small shaded solids. Colours are placeholders that the app
# replaces with the current line-style and theme colours (src/ui/iconraster.h):
#   currentColor  UI chrome (motion arrows, axes of revolution)
#   #e00001       sketch entity (style ACTIVE_GRP)
#   #e00002       point (style DATUM)
#   #e00003       constraint mark (style CONSTRAINT)
#   #e00004       construction (style CONSTRUCTION)
#   #e00005/6/7   solid faces: lit, mid, shaded
# image comes from Lucide (currentColor) and is not written here.

import math

ENTITY, POINT, CONSTRAINT, CONSTRUCTION = '#e00001', '#e00002', '#e00003', '#e00004'
LIT, MID, DARK = '#e00005', '#e00006', '#e00007'

HEAD = ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" '
        'fill="none" stroke-linecap="round" stroke-linejoin="round">')


def line(d, w=1.6):
    return f'<path d="{d}" stroke="{ENTITY}" stroke-width="{w}" />'


def mark(d, w=1.4):
    return f'<path d="{d}" stroke="{CONSTRAINT}" stroke-width="{w}" />'


def chrome(d, w=1.6, dash=None):
    extra = f' stroke-dasharray="{dash}"' if dash else ''
    return f'<path d="{d}" stroke="currentColor" stroke-width="{w}"{extra} />'


def pt(x, y, s=3.4):
    return f'<rect x="{x - s / 2:g}" y="{y - s / 2:g}" width="{s:g}" height="{s:g}" fill="{POINT}" />'


def arrowhead(x, y, dx, dy, size=2.2, stroke=None):
    """A V-shaped arrowhead at (x, y) pointing along (dx, dy)."""
    n = math.hypot(dx, dy)
    dx, dy = dx / n, dy / n
    px, py = -dy, dx
    ax, ay = x - dx * size + px * size * 0.7, y - dy * size + py * size * 0.7
    bx, by = x - dx * size - px * size * 0.7, y - dy * size - py * size * 0.7
    return f'M{ax:.2f} {ay:.2f} {x:.2f} {y:.2f} {bx:.2f} {by:.2f}'


# Small stroked letters and digits for constraint labels, `h` tall at (x, y) top-left.
def glyph(c, x, y, h=4.0):
    w = h * 0.6
    if c == 'H': return f'M{x} {y}v{h} M{x + w} {y}v{h} M{x} {y + h / 2}h{w}'
    if c == 'V': return f'M{x} {y}l{w / 2} {h} {w / 2} {-h}'
    if c == 'R': return (f'M{x} {y + h}V{y}h{w * 0.6}a{w * 0.45} {h / 4} 0 0 1 0 {h / 2}H{x} '
                         f'M{x + w * 0.45} {y + h / 2}L{x + w} {y + h}')
    if c == 'E': return f'M{x + w} {y}H{x}v{h}h{w} M{x} {y + h / 2}h{w * 0.8}'
    if c == 'F': return f'M{x + w} {y}H{x}v{h} M{x} {y + h / 2}h{w * 0.8}'
    if c == '1': return f'M{x + w / 2} {y}v{h} M{x + w * 0.1} {y + h * 0.25}L{x + w / 2} {y}'
    if c == '0': return f'M{x + w / 2} {y}a{w / 2} {h / 2} 0 1 0 0.01 0'
    raise ValueError(c)


def text(s, x, y, h=4.0, gap=1.1):
    out = []
    for c in s:
        out.append(glyph(c, x, y, h))
        x += h * 0.6 + gap
    return ' '.join(out)


def cube(x, y, s, stroke=DARK):
    """A small shaded cube; (x, y) is the front face's top-left corner."""
    d = s * 0.45
    front = f'M{x} {y}h{s}v{s}h{-s}z'
    top = f'M{x} {y}l{d} {-d}h{s}l{-d} {d}z'
    side = f'M{x + s} {y}l{d} {-d}v{s}l{-d} {d}z'
    return (f'<path d="{top}" fill="{LIT}" stroke="{stroke}" stroke-width="0.8" />'
            f'<path d="{side}" fill="{DARK}" stroke="{stroke}" stroke-width="0.8" />'
            f'<path d="{front}" fill="{MID}" stroke="{stroke}" stroke-width="0.8" />')


def offset_ticks(x0, y0, x1, y1, off, length=4.0, count=2, spacing=1.5):
    """Short marks parallel to a line, beside its middle (the canvas's parallel mark)."""
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    dx, dy = x1 - x0, y1 - y0
    n = math.hypot(dx, dy)
    dx, dy = dx / n, dy / n
    nx, ny = -dy, dx
    out = []
    for i in range(count):
        o = off + i * spacing
        cx, cy = mx + nx * o, my + ny * o
        out.append(f'M{cx - dx * length / 2:.2f} {cy - dy * length / 2:.2f}'
                   f'L{cx + dx * length / 2:.2f} {cy + dy * length / 2:.2f}')
    return ' '.join(out)


def dimension(y_line, y_dim, label):
    """A measured line with the canvas's |<- label ->| above it."""
    return (line(f'M5 {y_line}h14') + pt(5, y_line) + pt(19, y_line)
            + mark(f'M5 {y_line - 2.5}V{y_dim - 2} M19 {y_line - 2.5}V{y_dim - 2}', 1.0)
            + mark(f'M5 {y_dim}h{2.5} M19 {y_dim}h-2.5 '
                   + arrowhead(5, y_dim, -1, 0, 1.8) + ' ' + arrowhead(19, y_dim, 1, 0, 1.8), 1.2)
            + mark(text(label, 12 - (len(label) * 4.2 - 1.1) / 2, y_dim - 3.5, 7.0), 1.3))



def ellipse_pt(cx, cy, rx, ry, deg):
    t = math.radians(deg)
    return cx + rx * math.cos(t), cy + ry * math.sin(t)


def revolve_icon():
    """A round solid with a wedge cut out: a partial revolution (lathe is the full one)."""
    cx, top, h, rx, ry = 12, 7.5, 9, 8.5, 3.2
    a0, a1 = -25, 65                      # the cut-out wedge, front right (y grows downwards)
    p0 = ellipse_pt(cx, top, rx, ry, a0)
    p1 = ellipse_pt(cx, top, rx, ry, a1)
    left = (cx - rx, top)
    f = lambda p, dy=0: f'{p[0]:.2f} {p[1] + dy:.2f}'
    stroke = f'stroke="{DARK}" stroke-width="0.8"'
    side = (f'<path d="M{f(p1)}V{p1[1] + h:.2f}A{rx} {ry} 0 0 1 {f(left, h)}V{top}'
            f'A{rx} {ry} 0 0 0 {f(p1)}z" fill="{MID}" {stroke} />')
    cut1 = f'<path d="M{cx} {top}L{f(p0)}v{h}L{cx} {top + h}z" fill="{MID}" {stroke} />'
    cut2 = f'<path d="M{cx} {top}L{f(p1)}v{h}L{cx} {top + h}z" fill="{DARK}" {stroke} />'
    topface = (f'<path d="M{cx} {top}L{f(p1)}A{rx} {ry} 0 1 1 {f(p0)}z" fill="{LIT}" {stroke} />')
    return (side + cut1 + cut2 + topface + chrome(f'M{cx} 1v22', 1.2, '2 1.8'))


def helix_icon():
    """A coiled tube around its axis: back strands dark and thin, front strands lit and thick."""
    cx, rx, tilt, y0, pitch, turns = 12, 7.5, 3.4, 4.5, 7.5, 2.0
    pts = []
    steps = int(turns * 48)
    for i in range(steps + 1):
        t = 2 * math.pi * turns * i / steps
        pts.append((cx + rx * math.cos(t), y0 + pitch * t / (2 * math.pi) + tilt * math.sin(t),
                    math.sin(t) >= 0))
    def runs(front):
        out, cur = [], []
        for x, y, f in pts:
            if f == front:
                cur.append(f'{x:.2f} {y:.2f}')
            elif cur:
                out.append('M' + ' L'.join(cur)); cur = []
        if cur: out.append('M' + ' L'.join(cur))
        return ' '.join(out)
    back, front = runs(False), runs(True)
    return (f'<path d="{back}" stroke="{MID}" stroke-width="1.6" opacity="0.8" />'
            + chrome(f'M{cx} 1v22', 1.2, '2 1.8')
            + f'<path d="{front}" stroke="{DARK}" stroke-width="3.4" />'
            + f'<path d="{front}" stroke="{LIT}" stroke-width="1.8" />')

ICONS = {
    # Sketch: entities as the canvas draws them.
    'line': line('M5 19 19 5') + pt(5, 19) + pt(19, 5),
    'rectangle': line('M4 6h16v12H4z') + pt(4, 6) + pt(20, 6) + pt(20, 18) + pt(4, 18),
    'circle': line('M20 12a8 8 0 1 1-16 0 8 8 0 0 1 16 0') + pt(12, 12),
    'arc': line('M4 17A8 8 0 0 1 20 17') + pt(4, 17) + pt(20, 17) + pt(12, 17, 2.6),
    'tangent-arc': (line('M3 19h8a7 7 0 0 0 7-7V8') + pt(3, 19) + pt(11, 19) + pt(18, 8)
                    + mark('M7 12.5h3 M8.5 12.5v4', 1.2)),
    'spline': (f'<path d="M4 18 7 5 M20 6 17 19" stroke="{CONSTRUCTION}" stroke-width="1" '
               'stroke-dasharray="1.5 1.5" />'
               + line('M4 18C8 4 16 20 20 6') + pt(4, 18) + pt(20, 6) + pt(7, 5, 2.6) + pt(17, 19, 2.6)),
    'point': pt(12, 12, 7),
    'text': line('M5 20 12 4l7 16 M8 14h8') + pt(5, 20, 2.6),
    'construction': (f'<path d="M5 19 19 5" stroke="{CONSTRUCTION}" stroke-width="1.6" '
                     'stroke-dasharray="3 2.5" />' + pt(5, 19) + pt(19, 5)),
    'split': (line('M4 20 20 4 M4 4 20 20') + pt(4, 20) + pt(20, 4) + pt(4, 4) + pt(20, 20)
              + pt(12, 12, 4.2)),
    # Constrain: the marks the canvas draws, beside the entities they constrain.
    'dimension': dimension(20, 9, '10'),
    'angle': (line('M4 19h16 M4 19 15 6') + pt(4, 19) + pt(20, 19) + pt(15, 6)
              + mark('M13 19a9 9 0 0 0-2.6-6.3 ' + arrowhead(13, 19, 0, 1, 1.8)
                     + ' ' + arrowhead(10.4, 12.7, -0.7, -0.7, 1.8), 1.2)),
    'horizontal': line('M4 17h16') + pt(4, 17) + pt(20, 17) + mark(glyph('H', 10.5, 5, 7), 1.5),
    'vertical': line('M15 4v16') + pt(15, 4) + pt(15, 20) + mark(glyph('V', 4.5, 8.5, 7), 1.5),
    'parallel': (line('M3 12 13 4 M11 20 21 12') + pt(3, 12) + pt(13, 4) + pt(11, 20) + pt(21, 12)
                 + mark(offset_ticks(3, 12, 13, 4, 2.4) + ' ' + offset_ticks(11, 20, 21, 12, 2.4), 1.1)),
    'perpendicular': (line('M4 20h16 M9 20V4') + pt(4, 20) + pt(20, 20) + pt(9, 4)
                      + mark('M13 14h6 M16 14V8', 1.4)),
    'point-on': (line('M3 15h18') + pt(3, 15) + pt(21, 15) + pt(12, 15)
                 + mark('M9 12h6v6H9z', 1.2)),
    'symmetric': (f'<path d="M12 3v18" stroke="{CONSTRUCTION}" stroke-width="1.2" '
                  'stroke-dasharray="2 2" />' + pt(4, 12) + pt(20, 12)
                  + mark('M6.5 12h3.5 ' + arrowhead(10, 12, 1, 0, 2)
                         + ' M17.5 12H14 ' + arrowhead(14, 12, -1, 0, 2), 1.4)),
    'equal': (line('M4 7h16 M4 17h16') + pt(4, 7) + pt(20, 7) + pt(4, 17) + pt(20, 17)
              + mark('M12 4.5v5 M12 14.5v5', 1.4)),
    'same-orientation': (line('M3 19h7 M14 19h7') + pt(3, 19) + pt(10, 19) + pt(14, 19) + pt(21, 19)
                         + mark('M6.5 16V5 ' + arrowhead(6.5, 4.5, 0, -1, 2.2)
                                + ' M17.5 16V5 ' + arrowhead(17.5, 4.5, 0, -1, 2.2), 1.4)),
    'other-angle': (line('M3 17 21 7 M3 7 21 17') + pt(3, 17) + pt(21, 7) + pt(3, 7) + pt(21, 17)
                    + mark('M7.5 9.6a5.2 5.2 0 0 0 0 4.8', 1.4)),
    'reference': (line('M4 19h16') + pt(4, 19) + pt(20, 19)
                  + mark(text('REF', 3.6, 5, 8, 1.4), 1.6)),
    # Groups: what each makes, as a small shaded solid.
    'extrude': (line('M3 21h11l4-3', 1.2) + cube(3, 11, 10)
                + chrome('M10.75 7.5V1.5 ' + arrowhead(10.75, 1.2, 0, -1, 2.2))),
    'lathe': (f'<path d="M6 7v10a6 2.4 0 0 0 12 0V7" fill="{MID}" stroke="{DARK}" stroke-width="0.8" />'
              f'<path d="M12 7v12.4a6 2.4 0 0 0 6-2.4V7z" fill="{DARK}" opacity="0.6" />'
              f'<ellipse cx="12" cy="7" rx="6" ry="2.4" fill="{LIT}" stroke="{DARK}" stroke-width="0.8" />'
              + chrome('M12 1v22', 1.2, '2 1.8')
              + chrome('M3 12.5a9 3.2 0 1 0 3-2.6 ' + arrowhead(5.8, 10.0, 1, -0.5, 2), 1.3)),
    'revolve': revolve_icon(),
    'helix': helix_icon(),
    'circular-pattern': (chrome('M12 4.5a7.5 7.5 0 1 1-6.5 3.75', 1.1, '2 1.6')
                         + cube(9.5, 1.5, 4.4) + cube(16.5, 13.5, 4.4) + cube(1.5, 13.5, 4.4)),
    'linear-pattern': (cube(1.5, 8, 4.6) + cube(9, 8, 4.6) + cube(16.5, 8, 4.6)
                       + chrome('M2 18h18 ' + arrowhead(21, 18, 1, 0, 2.4))),
    'new-sketch': (f'<path d="M2 19l5-8h15l-5 8z" fill="{LIT}" opacity="0.55" stroke="{DARK}" '
                   'stroke-width="0.8" />'
                   + line('M7 17 16 12', 1.4) + pt(7, 17, 2.6) + pt(16, 12, 2.6)
                   + chrome('M15 2l3 3-6 6-3.5 1 1-3.5z', 1.3)),
    'sketch-3d': ('<path d="M7 17V3" stroke="#3a7bff" stroke-width="1.6" />'
                  '<path d="M7 17h14" stroke="#e8443a" stroke-width="1.6" />'
                  '<path d="M7 17l-5 5" stroke="#33b24a" stroke-width="1.6" />'
                  + line('M11 13 18 6', 1.4) + pt(11, 13, 2.6) + pt(18, 6, 2.6)),
    'normal-to': (f'<path d="M3 19l4-6h14l-4 6z" fill="{LIT}" opacity="0.55" stroke="{DARK}" '
                  'stroke-width="0.8" />'
                  + line('M8 17.5 15 14.5', 1.4) + pt(8, 17.5, 2.4) + pt(15, 14.5, 2.4)
                  + chrome('M12 1.5v10 ' + arrowhead(12, 12, 0, 1, 2.4))),
    'isometric': cube(5, 10, 11),
    'insert-part': cube(3, 11, 10) + chrome('M19 2v7 M15.5 5.5h7', 1.8),
}

if __name__ == '__main__':
    for name, body in ICONS.items():
        with open(f'{name}.svg', 'w') as f:
            f.write(HEAD + '\n  ' + body + '\n</svg>\n')
    print(f'{len(ICONS)} icons written')
