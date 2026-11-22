#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# gen-cursors.py OUTDIR: Atlas's cursors, drawn here from their geometry
# (a 32-unit grid, y down): a black glyph with a white outline and a soft
# shadow, colored badges where a cursor has one; the hands are white with
# a black outline; the busy wheel turns in 12 frames. Each is sampled 4 x 4 per
# pixel (antialiased) and written at 1x (32 px) and 2x (64 px) as PNG
# (NAME.png, NAME@2x.png), with Cursors.plist (each one's hot spot, in
# 1x pixels from the top left) and, for the window server, arrowCursor.bgra
# (its default arrow) and busyCursor_NN.bgra (the frames of the busy cursor it
# shows over an app that does not answer): width, height, hot x, hot y as 32-bit little-endian,
# then premultiplied BGRA rows). No third-party artwork: Python's standard
# library only.

import math
import os
import struct
import sys
import zlib

UNITS = 32
SAMPLES = 4             # per pixel, on each axis
OUTLINE = 1.45          # units around the glyph
OUTLINE_DARK = 1.05     # a black outline, around a white glyph
SHADOW_DY = 0.9         # units down
SHADOW_BLUR = 1.3       # units
SHADOW_ALPHA = 0.34

BLACK = (0.0, 0.0, 0.0)
WHITE = (1.0, 1.0, 1.0)
RED = (0.85, 0.20, 0.17)
GREEN = (0.20, 0.66, 0.30)
BLUE = (0.16, 0.48, 0.86)
GRAY = (0.33, 0.36, 0.40)

# ---- Shapes: a test (x, y) -> inside, and a bounding box --------------------


class Shape:
    def __init__(self, test, box):
        self.test, self.box = test, box


def polygon(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    n = len(points)

    def test(x, y):
        inside = False
        j = n - 1
        for i in range(n):
            xi, yi = points[i]
            xj, yj = points[j]
            if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                inside = not inside
            j = i
        return inside
    return Shape(test, (min(xs), min(ys), max(xs), max(ys)))


def circle(cx, cy, r):
    return Shape(lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r,
                 (cx - r, cy - r, cx + r, cy + r))


def segment_distance(x, y, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy) if dx or dy else 0
    t = max(0.0, min(1.0, t))
    return math.hypot(x - (ax + t * dx), y - (ay + t * dy))


def line(points, width):
    """A polyline of round-capped segments, width wide."""
    r = width / 2
    segs = list(zip(points, points[1:]))
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]

    def test(x, y):
        return any(segment_distance(x, y, a[0], a[1], b[0], b[1]) <= r for a, b in segs)
    return Shape(test, (min(xs) - r, min(ys) - r, max(xs) + r, max(ys) + r))


def round_rect(x0, y0, x1, y1, r):
    def test(x, y):
        if x < x0 or x > x1 or y < y0 or y > y1:
            return False
        cx = min(max(x, x0 + r), x1 - r)
        cy = min(max(y, y0 + r), y1 - r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    return Shape(test, (x0, y0, x1, y1))


def transformed(shape, angle, cx, cy):
    """shape turned by angle degrees about (cx, cy)."""
    c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))

    def test(x, y):
        dx, dy = x - cx, y - cy
        return shape.test(cx + dx * c + dy * s, cy - dx * s + dy * c)
    x0, y0, x1, y1 = shape.box
    corners = [(x0, y0), (x1, y0), (x0, y1), (x1, y1)]
    turned = [(cx + (px - cx) * c - (py - cy) * s, cy + (px - cx) * s + (py - cy) * c)
              for px, py in corners]
    return Shape(test, (min(p[0] for p in turned), min(p[1] for p in turned),
                        max(p[0] for p in turned), max(p[1] for p in turned)))


def scaled(shape, g):
    """shape scaled by g about the grid's origin (its top left)."""
    x0, y0, x1, y1 = shape.box
    return Shape(lambda x, y: shape.test(x / g, y / g), (x0 * g, y0 * g, x1 * g, y1 * g))


def union(*shapes):
    return Shape(lambda x, y: any(s.box[0] <= x <= s.box[2] and s.box[1] <= y <= s.box[3]
                                  and s.test(x, y) for s in shapes),
                 (min(s.box[0] for s in shapes), min(s.box[1] for s in shapes),
                  max(s.box[2] for s in shapes), max(s.box[3] for s in shapes)))

# ---- The glyphs ----------------------------------------------------------------


def arrow(scale=1.0, ox=0.0, oy=0.0):
    pts = [(6, 4), (6, 23.2), (10.6, 18.9), (13.6, 26), (16.9, 24.6), (14.0, 17.7), (20.2, 17.7)]
    return polygon([(ox + 6 + (x - 6) * scale, oy + 4 + (y - 4) * scale) for x, y in pts])


def double_arrow(angle):
    shaft = line([(9.5, 16), (22.5, 16)], 2.6)
    left = polygon([(3.6, 16), (10.6, 10.4), (10.6, 21.6)])
    right = polygon([(28.4, 16), (21.4, 10.4), (21.4, 21.6)])
    return transformed(union(shaft, left, right), angle, 16, 16)


def ibeam(angle=0):
    stem = line([(16, 8.2), (16, 23.8)], 1.9)
    top = line([(12.3, 6.4), (14.4, 6.4), (16, 8.0), (17.6, 6.4), (19.7, 6.4)], 1.7)
    bottom = line([(12.3, 25.6), (14.4, 25.6), (16, 24.0), (17.6, 25.6), (19.7, 25.6)], 1.7)
    bar = line([(14.6, 16), (17.4, 16)], 1.4)
    return transformed(union(stem, top, bottom, bar), angle, 16, 16)


def hand(pointing, closed=False):
    palm = round_rect(10.4, 14.6, 24.6, 27.4, 4.2)
    parts = [palm]
    if pointing:
        parts.append(line([(13.4, 5.6), (13.4, 17)], 3.6))           # the index
        parts += [line([(16.9, 12.4), (16.9, 17)], 3.4),
                  line([(20.2, 13.4), (20.2, 17)], 3.3),
                  line([(23.0, 15.4), (23.0, 18)], 3.0),
                  line([(8.6, 18.8), (11.8, 22.6)], 3.4)]              # the thumb
    elif closed:
        parts += [line([(13.3, 13.6), (13.3, 16)], 3.3),
                  line([(16.6, 12.9), (16.6, 16)], 3.3),
                  line([(19.9, 13.2), (19.9, 16)], 3.3),
                  line([(22.9, 14.6), (22.9, 17)], 3.0),
                  line([(9.6, 18.6), (11.6, 20.8)], 3.2)]
    else:
        parts += [line([(12.6, 6.8), (13.4, 16)], 3.2),
                  line([(16.6, 4.8), (16.8, 15)], 3.2),
                  line([(20.4, 5.8), (20.0, 15)], 3.2),
                  line([(23.9, 9.0), (22.9, 17)], 3.0),
                  line([(6.4, 15.4), (11.6, 21.6)], 3.2)]
    return union(*parts)


def hand_details(pointing, closed=False):
    """The white lines between the fingers."""
    if pointing:
        xs, y0 = (15.15, 18.55, 21.6), 17.8
    elif closed:
        xs, y0 = (14.95, 18.25, 21.4), 16.4
    else:
        xs, y0 = (14.9, 18.6, 21.9), 16.4
    return union(*[line([(x, y0), (x, y0 + 3.6)], 0.75) for x in xs])


SPINNER_FRAMES = 12
SPINNER_INTERVAL = 0.08  # seconds a frame


def spinner(cx, cy, r, frame):
    """A wheel of spokes turning, frame of SPINNER_FRAMES: a black disk and
    its spokes, the newest white, the others fading to gray behind it."""
    layers = [(circle(cx, cy, r), BLACK)]
    for i in range(SPINNER_FRAMES):
        angle = 2 * math.pi * i / SPINNER_FRAMES - math.pi / 2
        age = (frame - i) % SPINNER_FRAMES          # 0: the newest
        level = 1.0 - 0.68 * age / (SPINNER_FRAMES - 1)
        a, b = 0.42 * r, 0.80 * r
        layers.append((line([(cx + a * math.cos(angle), cy + a * math.sin(angle)),
                             (cx + b * math.cos(angle), cy + b * math.sin(angle))], 0.15 * r),
                       (level, level, level)))
    return layers


def badge(color, cx=22.4, cy=22.4, r=6.0):
    return (circle(cx, cy, r), color)

# ---- The cursors: layers (shape, color) from the bottom, and the hot spot -----


def cursors():
    c = {}
    c['arrowCursor'] = ([(arrow(), BLACK)], (6, 4))
    c['IBeamCursor'] = ([(ibeam(), BLACK)], (16, 16))
    c['IBeamCursorForVerticalLayout'] = ([(ibeam(90), BLACK)], (16, 16))
    c['crosshairCursor'] = ([(union(line([(16, 4.5), (16, 13.6)], 1.8), line([(16, 18.4), (16, 27.5)], 1.8),
                                    line([(4.5, 16), (13.6, 16)], 1.8), line([(18.4, 16), (27.5, 16)], 1.8),
                                    circle(16, 16, 1.1)), BLACK)], (16, 16))
    c['resizeLeftRightCursor'] = ([(double_arrow(0), BLACK)], (16, 16))
    c['resizeUpDownCursor'] = ([(double_arrow(90), BLACK)], (16, 16))
    c['resizeNorthWestSouthEastCursor'] = ([(double_arrow(45), BLACK)], (16, 16))
    c['resizeNorthEastSouthWestCursor'] = ([(double_arrow(-45), BLACK)], (16, 16))
    # The hands: white, outlined in black (thinner), the fingers' lines black.
    c['pointingHandCursor'] = ([(hand(True), WHITE), (hand_details(True), BLACK)], (13.4, 4.2), BLACK)
    c['openHandCursor'] = ([(hand(False), WHITE), (hand_details(False), BLACK)], (16, 16), BLACK)
    c['closedHandCursor'] = ([(hand(False, True), WHITE), (hand_details(False, True), BLACK)], (16, 16), BLACK)
    small = arrow(0.82)
    c['operationNotAllowedCursor'] = ([(small, BLACK), badge(RED),
                                      (transformed(line([(18.8, 22.4), (26.0, 22.4)], 1.9), 45, 22.4, 22.4), WHITE)],
                                     (6, 4))
    c['dragCopyCursor'] = ([(small, BLACK), badge(GREEN),
                           (union(line([(19.2, 22.4), (25.6, 22.4)], 1.9), line([(22.4, 19.2), (22.4, 25.6)], 1.9)), WHITE)],
                          (6, 4))
    c['dragLinkCursor'] = ([(small, BLACK), badge(GRAY),
                           (union(line([(19.6, 25.2), (24.6, 20.2)], 1.6),
                                  polygon([(25.9, 18.9), (25.9, 23.0), (21.8, 18.9)])), WHITE)],
                          (6, 4))
    c['contextualMenuCursor'] = ([(small, BLACK), (round_rect(17.0, 15.6, 28.0, 27.6, 1.2), WHITE),
                                 (union(*[line([(18.9, y), (26.1, y)], 1.3) for y in (18.4, 21.6, 24.8)]), BLACK)],
                                (6, 4))
    c['helpCursor'] = ([(small, BLACK), badge(BLUE),
                       (union(line([(20.1, 20.2), (20.6, 18.8), (22.0, 18.1), (23.6, 18.6), (24.2, 20.0),
                                    (23.5, 21.3), (22.4, 22.0), (22.4, 23.4)], 1.5),
                              circle(22.4, 25.6, 0.95)), WHITE)],
                      (6, 4))
    c['disappearingItemCursor'] = ([(union(circle(11.5, 17.5, 4.6), circle(17.0, 13.5, 5.6), circle(22.0, 17.8, 4.4),
                                          circle(16.0, 20.5, 4.2), round_rect(9.0, 17.0, 24.5, 22.6, 2.6)), GRAY),
                                   (union(line([(13.5, 16.0), (18.5, 21.0)], 1.5), line([(18.5, 16.0), (13.5, 21.0)], 1.5)), WHITE)],
                                  (16, 16))
    # Busy (the app does not answer): an animation, NAME_NN for each frame.
    for f in range(SPINNER_FRAMES):
        c['busyCursor_%02d' % f] = (spinner(16, 16, 10.5, f), (16, 16))
    return c

# Each glyph's scale: its visible box (with the outline) as large as the
# reference's same cursor, measured on it (22 Nov 2026, the images
# NSCursor gives, in points): the arrow 11.5 x 18, the I-beam 8.4 x 17.4,
# the crosshair 15 x 15, the pointing hand 15 x 16.5, the open hand 16 x 16,
# the closed one 13.5 x 12.5, the resize arrows 17 x 16, the arrow with a
# badge 19 wide.
SCALES = {
    'arrowCursor': 0.75, 'IBeamCursor': 0.72, 'IBeamCursorForVerticalLayout': 0.67,
    'crosshairCursor': 0.54, 'pointingHandCursor': 0.63, 'openHandCursor': 0.64,
    'closedHandCursor': 0.71, 'resizeLeftRightCursor': 0.65, 'resizeUpDownCursor': 0.67,
    'resizeNorthWestSouthEastCursor': 0.62, 'resizeNorthEastSouthWestCursor': 0.62,
    'operationNotAllowedCursor': 0.76, 'dragCopyCursor': 0.76, 'dragLinkCursor': 0.76,
    'contextualMenuCursor': 0.70, 'helpCursor': 0.76, 'disappearingItemCursor': 0.75,
    'busyCursor': 0.72,
}


def scale_for(name):
    return SCALES.get(name.split('_')[0], 1.0)

# ---- Rendering -----------------------------------------------------------------


def coverage(shape, size, k):
    """shape sampled on a (size*k)^2 grid: a list of rows of 0/1."""
    n = size * k
    unit = UNITS / n
    mask = [[0] * n for _ in range(n)]
    x0, y0, x1, y1 = shape.box
    i0, i1 = max(0, int(y0 / unit) - 1), min(n, int(y1 / unit) + 2)
    j0, j1 = max(0, int(x0 / unit) - 1), min(n, int(x1 / unit) + 2)
    for i in range(i0, i1):
        y = (i + 0.5) * unit
        row = mask[i]
        for j in range(j0, j1):
            if shape.test((j + 0.5) * unit, y):
                row[j] = 1
    return mask


def distance(mask):
    """Each sample's distance, in samples, to the nearest set one (8-way chamfer)."""
    n = len(mask)
    big = 1e9
    d = [[0.0 if mask[i][j] else big for j in range(n)] for i in range(n)]
    a, b = 1.0, math.sqrt(2)
    for i in range(n):
        for j in range(n):
            v = d[i][j]
            if i > 0:
                v = min(v, d[i - 1][j] + a)
                if j > 0:
                    v = min(v, d[i - 1][j - 1] + b)
                if j < n - 1:
                    v = min(v, d[i - 1][j + 1] + b)
            if j > 0:
                v = min(v, d[i][j - 1] + a)
            d[i][j] = v
    for i in range(n - 1, -1, -1):
        for j in range(n - 1, -1, -1):
            v = d[i][j]
            if i < n - 1:
                v = min(v, d[i + 1][j] + a)
                if j < n - 1:
                    v = min(v, d[i + 1][j + 1] + b)
                if j > 0:
                    v = min(v, d[i + 1][j - 1] + b)
            if j < n - 1:
                v = min(v, d[i][j + 1] + a)
            d[i][j] = v
    return d


def box_blur(grid, r):
    """Three box blurs, rows then columns: about a Gaussian."""
    n = len(grid)
    r = max(1, int(round(r)))
    for _ in range(3):
        out = [[0.0] * n for _ in range(n)]
        for i in range(n):
            row = grid[i]
            s = sum(row[0:r + 1])
            for j in range(n):
                out[i][j] = s / (2 * r + 1)
                if j + r + 1 < n:
                    s += row[j + r + 1]
                if j - r >= 0:
                    s -= row[j - r]
        grid = out
        out = [[0.0] * n for _ in range(n)]
        for j in range(n):
            s = sum(grid[i][j] for i in range(0, min(n, r + 1)))
            for i in range(n):
                out[i][j] = s / (2 * r + 1)
                if i + r + 1 < n:
                    s += grid[i + r + 1][j]
                if i - r >= 0:
                    s -= grid[i - r][j]
        grid = out
    return grid


def render(layers, scale, outline_color=WHITE, g=1.0):
    """The cursor at scale (32 * scale pixels): premultiplied RGBA rows (0..1),
    outlined in outline_color (a black outline is thinner)."""
    size = UNITS * scale
    k = SAMPLES
    n = size * k
    per_unit = n / UNITS
    masks = [coverage(shape, size, k) for shape, _ in layers]
    whole = [[1 if any(m[i][j] for m in masks) else 0 for j in range(n)] for i in range(n)]
    d = distance(whole)
    # The outline and the shadow keep their size in pixels at any glyph scale.
    outline_r = (OUTLINE if outline_color == WHITE else OUTLINE_DARK) * per_unit * max(g, 0.7)
    outline = [[1.0 if d[i][j] <= outline_r else 0.0 for j in range(n)] for i in range(n)]
    # The shadow: the outlined shape, down, blurred.
    dy = int(round(SHADOW_DY * per_unit))
    shifted = [[outline[i - dy][j] if i - dy >= 0 else 0.0 for j in range(n)] for i in range(n)]
    shadow = box_blur(shifted, SHADOW_BLUR * per_unit / 1.7)
    # Composited per sample (premultiplied), then averaged per pixel.
    out = []
    for py in range(size):
        row = []
        for px in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for si in range(k):
                i = py * k + si
                for sj in range(k):
                    j = px * k + sj
                    a = shadow[i][j] * SHADOW_ALPHA
                    r = g = b = 0.0
                    if outline[i][j]:
                        r, g, b, a = outline_color[0], outline_color[1], outline_color[2], 1.0
                    for (shape, color), m in zip(layers, masks):
                        if m[i][j]:
                            r, g, b, a = color[0], color[1], color[2], 1.0
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            m = k * k
            row.append((acc[0] / m, acc[1] / m, acc[2] / m, acc[3] / m))
        out.append(row)
    return out

# ---- Files ---------------------------------------------------------------------


def png(rows, path):
    """Straight-alpha RGBA: the color is the premultiplied sum over coverage."""
    h, w = len(rows), len(rows[0])
    raw = bytearray()
    for row in rows:
        raw.append(0)
        for r, g, b, a in row:
            if a > 0:
                r, g, b = r / a, g / a, b / a
            raw += bytes((round(min(1, r) * 255), round(min(1, g) * 255),
                          round(min(1, b) * 255), round(a * 255)))

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))


def bgra(rows, hot, path):
    """Premultiplied BGRA, for the window server."""
    h, w = len(rows), len(rows[0])
    data = bytearray(struct.pack('<IIII', w, h, round(hot[0]), round(hot[1])))
    for row in rows:
        for r, g, b, a in row:
            data += bytes((round(min(1, b) * 255), round(min(1, g) * 255),
                           round(min(1, r) * 255), round(a * 255)))
    with open(path, 'wb') as f:
        f.write(data)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    plist = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">',
             '<plist version="1.0">', '<dict>']
    for name, entry in sorted(cursors().items()):
        g = scale_for(name)
        layers = [(scaled(shape, g), color) for shape, color in entry[0]]
        hot = (entry[1][0] * g, entry[1][1] * g)
        outline_color = entry[2] if len(entry) > 2 else WHITE
        one = render(layers, 1, outline_color, g)
        png(one, os.path.join(out, name + '.png'))
        png(render(layers, 2, outline_color, g), os.path.join(out, name + '@2x.png'))
        if name == 'arrowCursor' or name.startswith('busyCursor_'):
            bgra(one, hot, os.path.join(out, name + '.bgra'))
        plist += ['\t<key>%s</key>' % name, '\t<dict>',
                  '\t\t<key>hotX</key>', '\t\t<real>%g</real>' % hot[0],
                  '\t\t<key>hotY</key>', '\t\t<real>%g</real>' % hot[1], '\t</dict>']
    for anim in ('busyCursor',):
        plist += ['\t<key>%s</key>' % anim, '\t<dict>',
                  '\t\t<key>frames</key>', '\t\t<integer>%d</integer>' % SPINNER_FRAMES,
                  '\t\t<key>interval</key>', '\t\t<real>%g</real>' % SPINNER_INTERVAL,
                  '\t\t<key>hotX</key>', '\t\t<real>%g</real>' % (16 * scale_for(anim)),
                  '\t\t<key>hotY</key>', '\t\t<real>%g</real>' % (16 * scale_for(anim)),
                  '\t</dict>']
    plist += ['</dict>', '</plist>', '']
    with open(os.path.join(out, 'Cursors.plist'), 'w') as f:
        f.write('\n'.join(plist))


if __name__ == '__main__':
    main()
