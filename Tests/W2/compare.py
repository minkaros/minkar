#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W2: compare a rendering of page.html with its reference.
#
#   compare.py RENDERED.ppm OUTDIR
#
# The reference is made here from the page's geometry (REGIONS, painted in
# order on a white page): each of its pixels must match the rendering
# within TOLERANCE per channel, except in the text's box, where the
# rendering must have ink (dark pixels) and nothing else. OUTDIR gets
# reference.ppm and diff.ppm (the pixels that differ, in red). Prints
# "w2-compare: PASS" or "w2-compare: FAIL" and exits 0 or 1.
import sys

WIDTH, HEIGHT = 800, 600
TOLERANCE = 2
MIN_INK = 400          # dark pixels the text must leave at the least


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def over(src, alpha, dst):
    return tuple(round(s * alpha + d * (1 - alpha)) for s, d in zip(src, dst))


HEADER = rgb('#2c5f7a')
# (left, top, width, height, color), in page.html's order.
REGIONS = [
    (0, 0, 800, 60, HEADER),
    (600, 0, 200, 60, over((0, 0, 255), 0.5, HEADER)),
    (40, 80, 200, 120, rgb('#e74c3c')),
    (300, 80, 200, 120, rgb('#27ae60')),
    (560, 80, 200, 120, rgb('#f1c40f')),
    (40, 240, 180, 80, rgb('#3498db')),
    (240, 240, 180, 80, rgb('#e67e22')),
    (40, 340, 180, 80, rgb('#1abc9c')),
    (240, 340, 180, 80, rgb('#95a5a6')),
    (480, 240, 260, 180, (0, 0, 0)),
    (490, 250, 240, 160, rgb('#8e44ad')),
    (240, 340, 40, 40, rgb('#c0392b')),
]
TEXT = (40, 460, 720, 100)


def read_ppm(path):
    """(width, height, pixels) of an 8-bit P6 image, or None."""
    data = open(path, 'rb').read()
    fields, pos = [], 0
    while len(fields) < 4 and pos < len(data):
        if data[pos:pos + 1].isspace():
            pos += 1
        elif data[pos:pos + 1] == b'#':
            end = data.find(b'\n', pos)
            pos = len(data) if end < 0 else end
        else:
            end = pos
            while end < len(data) and not data[end:end + 1].isspace():
                end += 1
            fields.append(data[pos:end])
            pos = end
    if len(fields) < 4 or fields[0] != b'P6' or fields[3] != b'255' or \
            not fields[1].isdigit() or not fields[2].isdigit():
        return None
    w, h = int(fields[1]), int(fields[2])
    pixels = data[pos + 1:pos + 1 + 3 * w * h]
    if len(pixels) != 3 * w * h:
        return None
    return w, h, pixels


def write_ppm(path, w, h, pixels):
    with open(path, 'wb') as f:
        f.write(b'P6\n%d %d\n255\n' % (w, h))
        f.write(bytes(pixels))


def main():
    rendered, outdir = sys.argv[1], sys.argv[2]
    image = read_ppm(rendered)
    if image is None:
        print(f'w2-compare: {rendered} is not a whole 8-bit P6 image')
        print('w2-compare: FAIL')
        return 1
    w, h, got = image
    if (w, h) != (WIDTH, HEIGHT):
        print(f'w2-compare: the rendering is {w}x{h}, not {WIDTH}x{HEIGHT}')
        print('w2-compare: FAIL')
        return 1

    ref = bytearray([255] * (3 * WIDTH * HEIGHT))
    for left, top, rw, rh, color in REGIONS:
        for y in range(top, top + rh):
            for x in range(left, left + rw):
                ref[3 * (y * WIDTH + x):3 * (y * WIDTH + x) + 3] = bytes(color)
    write_ppm(f'{outdir}/reference.ppm', WIDTH, HEIGHT, ref)

    tl, tt, tw, th = TEXT
    diff = bytearray(ref)
    wrong = ink = 0
    for y in range(HEIGHT):
        for x in range(WIDTH):
            i = 3 * (y * WIDTH + x)
            g = got[i:i + 3]
            if tl <= x < tl + tw and tt <= y < tt + th:
                # Text: gray levels only (black on white, antialiased).
                if max(g) - min(g) > 2 * TOLERANCE + 8:
                    wrong += 1
                    diff[i:i + 3] = b'\xff\x00\x00'
                elif max(g) < 128:
                    ink += 1
                continue
            if any(abs(a - b) > TOLERANCE for a, b in zip(g, ref[i:i + 3])):
                wrong += 1
                diff[i:i + 3] = b'\xff\x00\x00'
    write_ppm(f'{outdir}/diff.ppm', WIDTH, HEIGHT, diff)

    print(f'w2-compare: {wrong} pixels differ from the reference; text ink {ink} pixels')
    ok = wrong == 0 and ink >= MIN_INK
    print('w2-compare: PASS' if ok else 'w2-compare: FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
