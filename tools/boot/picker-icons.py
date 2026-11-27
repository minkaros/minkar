# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# picker-icons.py OUTDIR: the boot picker's icons, drawn here (Minkar's own
# art; only the logo comes from Logo_Assets). Each is a raw image for the
# EFI loader: "MKBA", width and height (32-bit little-endian), then BGRA
# pixels (straight alpha), so the loader blends it over whatever is under
# it (the black screen, the chosen entry's highlight).
#
#   minkar.raw   a system that boots graphical: the logo on Atlas's blue
#   verbose.raw  verbose mode: a terminal's prompt on dark grey
#   disk.raw     another volume's loader (Windows, EFI): a drive
#   start.raw    the button under the chosen entry: an arrow in a ring
#   restart.raw, shutdown.raw  the buttons at the bottom
#
# Shapes are sampled 4x4 times per pixel. The icons are 256 pixels and the
# buttons 96; the loader scales them to the screen.

import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pngraw  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LOGO = os.path.join(HERE, '..', '..', 'Logo_Assets', 'Minkar', 'Boot', 'MinkarBootLogo-White-512.png')
SAMPLES = [((i + 0.5) / 4, (j + 0.5) / 4) for j in range(4) for i in range(4)]


class Canvas:
    def __init__(self, size):
        self.size = size
        self.px = [[0.0, 0.0, 0.0, 0.0] for _ in range(size * size)]   # premultiplied RGBA, 0..1

    def paint(self, inside, colour, box=None):
        """Over: colour (r, g, b, a) or a function of (x, y) where inside(x, y)
        holds, x and y in 0..1 of the canvas."""
        n = self.size
        x0, y0, x1, y1 = box or (0, 0, n, n)
        for y in range(max(0, int(y0)), min(n, int(math.ceil(y1)))):
            for x in range(max(0, int(x0)), min(n, int(math.ceil(x1)))):
                hits = 0
                for sx, sy in SAMPLES:
                    if inside((x + sx) / n, (y + sy) / n):
                        hits += 1
                if not hits:
                    continue
                c = colour((x + 0.5) / n, (y + 0.5) / n) if callable(colour) else colour
                a = c[3] * hits / 16
                p = self.px[y * n + x]
                for k in range(3):
                    p[k] = c[k] * a + p[k] * (1 - a)
                p[3] = a + p[3] * (1 - a)

    def image(self, path, scale):
        """A PNG with alpha drawn centred, scale of the canvas wide."""
        w, h, ch, rows = pngraw.read_png(path)
        n = self.size
        side = int(n * scale)
        left = (n - side) // 2
        for y in range(side):
            for x in range(side):
                # Average of the source pixels this one covers.
                sx0, sx1 = x * w // side, max(x * w // side + 1, (x + 1) * w // side)
                sy0, sy1 = y * h // side, max(y * h // side + 1, (y + 1) * h // side)
                r = g = b = a = 0
                count = 0
                for yy in range(sy0, sy1):
                    row = rows[yy]
                    for xx in range(sx0, sx1):
                        al = row[xx * ch + 3] / 255 if ch == 4 else 1
                        r += row[xx * ch] / 255 * al
                        g += row[xx * ch + 1] / 255 * al
                        b += row[xx * ch + 2] / 255 * al
                        a += al
                        count += 1
                if not count or a == 0:
                    continue
                r, g, b, a = r / count, g / count, b / count, a / count
                p = self.px[(left + y) * n + left + x]
                for k, c in enumerate((r, g, b)):
                    p[k] = c + p[k] * (1 - a)
                p[3] = a + p[3] * (1 - a)

    def save(self, path):
        n = self.size
        out = bytearray(b'MKBA' + struct.pack('<II', n, n))
        for r, g, b, a in self.px:
            if a > 0:
                r, g, b = r / a, g / a, b / a
            out += bytes(round(min(1, max(0, c)) * 255) for c in (b, g, r, a))
        open(path, 'wb').write(out)


def rgb(hex_colour, alpha=1.0):
    return tuple(int(hex_colour[i:i + 2], 16) / 255 for i in (0, 2, 4)) + (alpha,)


def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def ring(cx, cy, r, width):
    return lambda x, y: (r - width) ** 2 <= (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def segment(ax, ay, bx, by, width):
    """A stroke with round ends from a to b."""
    dx, dy = bx - ax, by - ay
    length = dx * dx + dy * dy

    def inside(x, y):
        t = max(0, min(1, ((x - ax) * dx + (y - ay) * dy) / length)) if length else 0
        px, py = ax + t * dx - x, ay + t * dy - y
        return px * px + py * py <= (width / 2) ** 2
    return inside


def arc(cx, cy, r, width, start, end):
    """A stroke along a circle, from angle start to end (degrees, clockwise
    from the top), with round ends."""
    def at(angle):
        a = math.radians(angle)
        return cx + r * math.sin(a), cy - r * math.cos(a)
    ends = [segment(*at(start), *at(start), width), segment(*at(end), *at(end), width)]

    def inside(x, y):
        d = math.hypot(x - cx, y - cy)
        if abs(d - r) <= width / 2:
            angle = math.degrees(math.atan2(x - cx, cy - y)) % 360
            if (angle - start) % 360 <= (end - start) % 360:
                return True
        return any(e(x, y) for e in ends)
    return inside


def rounded_rect(x0, y0, x1, y1, r):
    def inside(x, y):
        if not (x0 <= x <= x1 and y0 <= y <= y1):
            return False
        cx = min(max(x, x0 + r), x1 - r)
        cy = min(max(y, y0 + r), y1 - r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    return inside


def gradient(top, bottom):
    t, b = rgb(top), rgb(bottom)
    return lambda x, y: tuple(t[k] + (b[k] - t[k]) * y for k in range(3)) + (1.0,)


def disc(canvas, top, bottom):
    """The round base every entry's icon has, with a faint rim."""
    canvas.paint(circle(0.5, 0.5, 0.46), gradient(top, bottom))
    canvas.paint(ring(0.5, 0.5, 0.46, 0.012), rgb('ffffff', 0.18))


def minkar(path):
    c = Canvas(256)
    disc(c, '3f7fc4', '1f4b7f')
    c.image(LOGO, 0.62)
    c.save(path)


def verbose(path):
    c = Canvas(256)
    disc(c, '4a4a4e', '232326')
    white = rgb('f0f0f2')
    c.paint(segment(0.31, 0.37, 0.45, 0.50, 0.06), white)
    c.paint(segment(0.45, 0.50, 0.31, 0.63, 0.06), white)
    c.paint(segment(0.51, 0.64, 0.69, 0.64, 0.06), white)
    c.save(path)


def disk(path):
    c = Canvas(256)
    disc(c, '6e6e73', '38383c')
    c.paint(rounded_rect(0.25, 0.37, 0.75, 0.63, 0.05), gradient('e6e6ea', 'b4b4ba'))
    c.paint(segment(0.32, 0.52, 0.56, 0.52, 0.025), rgb('6e6e73'))
    c.paint(circle(0.66, 0.52, 0.025), rgb('31679e'))
    c.save(path)


def button(path, glyph):
    c = Canvas(96)
    white = rgb('f0f0f2')
    c.paint(ring(0.5, 0.5, 0.44, 0.045), white)
    for shape in glyph:
        c.paint(shape, white)
    c.save(path)


def main(argv):
    if len(argv) != 1:
        sys.exit('usage: picker-icons.py OUTDIR')
    out = argv[0]
    os.makedirs(out, exist_ok=True)
    minkar(os.path.join(out, 'minkar.raw'))
    verbose(os.path.join(out, 'verbose.raw'))
    disk(os.path.join(out, 'disk.raw'))
    w = 0.06
    button(os.path.join(out, 'start.raw'), [
        segment(0.5, 0.68, 0.5, 0.32, w), segment(0.5, 0.32, 0.36, 0.46, w), segment(0.5, 0.32, 0.64, 0.46, w)])
    button(os.path.join(out, 'restart.raw'), [
        arc(0.5, 0.52, 0.19, w, 70, 360), segment(0.55, 0.33, 0.46, 0.25, w), segment(0.55, 0.33, 0.46, 0.41, w)])
    button(os.path.join(out, 'shutdown.raw'), [
        arc(0.5, 0.53, 0.18, w, 40, 320), segment(0.5, 0.26, 0.5, 0.48, w)])


if __name__ == '__main__':
    main(sys.argv[1:])
