# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pngraw.py: the boot screen's images for the EFI loader, which draws them
# without a PNG decoder. Each 8-bit RGB or RGBA PNG (not interlaced) becomes
# a raw image: "MKBI", width and height (32-bit little-endian), then its
# pixels as BGRX rows, each pixel already blended over the background
# colour given (the boot screen's, so the loader just copies them).
#
#   pngraw.py [--background RRGGBB] IN.png OUT.raw
#
# Also a module: read_png(path) -> (width, height, channels, rows).

import struct
import sys
import zlib


def read_png(path):
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'{path}: not a PNG')
    pos, idat, width = 8, b'', None
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            width, height, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', body)
            if depth != 8 or colour not in (2, 6) or interlace:
                raise ValueError(f'{path}: only 8-bit RGB or RGBA, not interlaced')
            channels = 4 if colour == 6 else 3
        elif kind == b'IDAT':
            idat += body
        elif kind == b'IEND':
            break
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            if kind == 1:
                line[i] = (line[i] + a) & 0xff
            elif kind == 2:
                line[i] = (line[i] + b) & 0xff
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xff
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xff
        rows.append(bytes(line))
        previous = line
    return width, height, channels, rows


def main(argv):
    background = (0, 0, 0)
    if len(argv) > 2 and argv[0] == '--background':
        background = tuple(int(argv[1][i:i + 2], 16) for i in (0, 2, 4))
        argv = argv[2:]
    if len(argv) != 2:
        sys.exit('usage: pngraw.py [--background RRGGBB] IN.png OUT.raw')
    width, height, channels, rows = read_png(argv[0])
    out = bytearray(b'MKBI' + struct.pack('<II', width, height))
    for row in rows:
        for x in range(width):
            r, g, b = row[x * channels:x * channels + 3]
            alpha = row[x * channels + 3] if channels == 4 else 255
            blend = lambda c, k: (c * alpha + k * (255 - alpha) + 127) // 255
            out += bytes((blend(b, background[2]), blend(g, background[1]), blend(r, background[0]), 0))
    open(argv[1], 'wb').write(out)


if __name__ == '__main__':
    main(sys.argv[1:])
