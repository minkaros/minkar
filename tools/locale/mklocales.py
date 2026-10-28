#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
"""Builds Minkar's UTF-8 locales (/usr/share/locale) for Darwin's libc.

  mklocales.py --data DIR --out DIR

DIR (--data) holds Unicode's UnicodeData.txt and EastAsianWidth.txt
(unicode/) and FreeBSD's locale definitions (timedef/, monetdef/,
numericdef/, msgdef/: their .src files and Makefiles, from CLDR).

It writes:
  UTF-8/LC_CTYPE    the UTF-8 character classes, case maps and widths,
                    in the format Darwin's libc reads ("RuneMagA": the
                    32-bit _RuneLocale with big-endian integers, then the
                    ranges for characters above 255)
  UTF-8/LC_COLLATE  collation by code point ("1.1A" format)
  <ll_CC>.UTF-8/    for each language and country FreeBSD defines all of
                    LC_TIME, LC_MONETARY, LC_NUMERIC and LC_MESSAGES for:
                    those four as text (LC_MESSAGES/LC_MESSAGES, as macOS
                    lays it out) (from FreeBSD's UTF-8 definition,
                    or its 8-bit one converted to UTF-8), and LC_CTYPE and
                    LC_COLLATE as links to UTF-8's

The output depends only on the input files: the same data gives the same
bytes.
"""

import argparse
import os
import re
import struct
import sys

# _CTYPE_* (Darwin's <_ctype.h>)
A, C, D, G, L, P, S, U = 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
X, B, R, I = 0x10000, 0x20000, 0x40000, 0x80000
SW0, SW2 = 0x20000000, 0x80000000
CACHED = 256
INVALID_RUNE = 0xFFFD
NO_BREAK_SPACES = {0x00A0, 0x2007, 0x202F}


def read_unicode(path):
    """Code point -> (category, upper, lower), ranges expanded."""
    chars = {}
    first = None
    with open(path, encoding='ascii') as f:
        for line in f:
            fields = line.rstrip('\n').split(';')
            cp, name, cat = int(fields[0], 16), fields[1], fields[2]
            upper = int(fields[12], 16) if fields[12] else None
            lower = int(fields[13], 16) if fields[13] else None
            if name.endswith(', First>'):
                first = cp
                continue
            if name.endswith(', Last>'):
                for c in range(first, cp + 1):
                    chars[c] = (cat, None, None, name)
                continue
            chars[cp] = (cat, upper, lower, name)
    return chars


def read_widths(path):
    """Code points whose East Asian width is W (wide) or F (fullwidth)."""
    wide = set()
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            rng, width = [x.strip() for x in line.split(';')]
            if width not in ('W', 'F'):
                continue
            lo, _, hi = rng.partition('..')
            for c in range(int(lo, 16), int(hi or lo, 16) + 1):
                wide.add(c)
    return wide


def char_type(c, info, wide):
    """The _CTYPE_ bits of one character (0: not a character)."""
    cat, _upper, _lower, name = info
    t = 0
    if cat == 'Cc':
        t |= C
        if 0x09 <= c <= 0x0D or c == 0x85:
            t |= S
        if c == 0x09:
            t |= B
        return t
    if cat in ('Cs', 'Co', 'Cn'):
        return 0
    if cat in ('Zl', 'Zp'):
        return C | S
    if cat == 'Zs':
        t |= R
        if c not in NO_BREAK_SPACES:
            t |= S | B
        return t | (SW2 if c in wide else 0)
    # Everything else prints.
    t |= R | G
    if cat[0] == 'L' or cat == 'Nl' or (cat == 'Nd' and c > 0x7F):
        t |= A
    if cat == 'Lu':
        t |= U
    if cat == 'Ll':
        t |= L
    if cat[0] in 'PS':
        t |= P
    if 0x30 <= c <= 0x39:
        t |= D | X | (c - 0x30)
    if 0x41 <= c <= 0x46 or 0x61 <= c <= 0x66:
        t |= X | ((c & 0x0F) + 9)
    if name.startswith(('<CJK Ideograph', 'CJK COMPATIBILITY IDEOGRAPH')):
        t |= I
    if cat in ('Mn', 'Me') or (cat == 'Cf' and c != 0x00AD) or \
            0x1160 <= c <= 0x11FF or c == 0x200B:
        t |= SW0
    elif c in wide:
        t |= SW2
    return t


def build_ctype(chars, wide):
    types = {c: char_type(c, info, wide) for c, info in chars.items()}
    upper = {c: info[1] for c, info in chars.items() if info[1] is not None}
    lower = {c: info[2] for c, info in chars.items() if info[2] is not None}

    runetype = [types.get(c, 0) for c in range(CACHED)]
    mapupper = [upper.get(c, c) for c in range(CACHED)]
    maplower = [lower.get(c, c) for c in range(CACHED)]

    def runs(table, step):
        items = sorted((c, v) for c, v in table.items() if c >= CACHED and v)
        out = []
        for c, v in items:
            if out:
                lo, hi, v0 = out[-1]
                if c == hi + 1 and v == v0 + (c - lo) * step:
                    out[-1] = (lo, c, v0)
                    continue
            out.append((c, c, v))
        return out

    type_ext = runs(types, 0)           # same type all along
    upper_ext = runs(upper, 1)          # the map rises with the character
    lower_ext = runs(lower, 1)

    encoding = b'UTF-8'.ljust(32, b'\0')
    head = b'RuneMagA' + encoding
    head += struct.pack('>III', 0, 0, INVALID_RUNE)       # sgetrune, sputrune
    head += struct.pack('>%dI' % CACHED, *runetype)
    head += struct.pack('>%dI' % CACHED, *maplower)
    head += struct.pack('>%dI' % CACHED, *mapupper)
    for ext in (type_ext, lower_ext, upper_ext):
        head += struct.pack('>II', len(ext), 0)
    head += struct.pack('>IIII', 0, 0, 0, 0)    # variable, len, classes
    assert len(head) == 3164, len(head)
    body = b''
    for ext in (type_ext, lower_ext, upper_ext):
        for lo, hi, v in ext:
            # A type range keeps its type in map (types pointer 0).
            body += struct.pack('>IIII', lo, hi, v, 0)
    return head + body


def build_collate():
    """Code point order: every character weighs its value plus one."""
    out = b'1.1A\n'.ljust(10, b'\0')
    directive_count, chain_max_len = 2, 0
    out += struct.pack('BBBB', 1, 1, 0,                 # forward, forward
                       directive_count | chain_max_len << 4)
    out += struct.pack('>ii', -1, -1)   # undefined: the value minus -1
    out += struct.pack('>ii', 0, 0)     # no substitutions
    out += struct.pack('>ii', 0, 0)     # no chains, no large table
    out += b''.join(struct.pack('>ii', c + 1, c + 1) for c in range(256))
    return out


# FreeBSD's encodings, as Python's codecs name them.
CODECS = {
    'UTF-8': 'utf-8', 'US-ASCII': 'ascii', 'ISO8859-1': 'iso8859-1',
    'ISO8859-2': 'iso8859-2', 'ISO8859-4': 'iso8859-4',
    'ISO8859-5': 'iso8859-5', 'ISO8859-7': 'iso8859-7',
    'ISO8859-9': 'iso8859-9', 'ISO8859-13': 'iso8859-13',
    'ISO8859-15': 'iso8859-15', 'KOI8-R': 'koi8-r', 'KOI8-U': 'koi8-u',
    'CP1251': 'cp1251', 'CP866': 'cp866', 'CP1131': 'cp1125',
    'Big5': 'big5', 'GB2312': 'gb2312', 'GBK': 'gbk', 'GB18030': 'gb18030',
    'eucCN': 'gb2312', 'eucJP': 'euc_jp', 'eucKR': 'euc_kr', 'SJIS':
    'shift_jis', 'Big5HKSCS': 'big5hkscs',
}
# Which encoding to convert from when there is no UTF-8 definition.
# The euro sign is in ISO8859-15, not in ISO8859-1.
PREFER = ['UTF-8', 'ISO8859-15', 'ISO8859-1', 'US-ASCII', 'ISO8859-2',
          'ISO8859-13', 'ISO8859-9', 'ISO8859-7', 'ISO8859-5', 'ISO8859-4',
          'KOI8-R', 'KOI8-U', 'CP1251', 'CP866', 'eucJP', 'SJIS', 'eucKR',
          'GBK', 'GB2312', 'eucCN', 'GB18030', 'Big5', 'Big5HKSCS',
          'CP1131']
CATEGORIES = {'timedef': 'LC_TIME', 'monetdef': 'LC_MONETARY',
              'numericdef': 'LC_NUMERIC', 'msgdef': 'LC_MESSAGES'}


def read_category(dirpath):
    """locale name -> the .src file that defines it (LOCALES and SAME)."""
    mk = open(os.path.join(dirpath, 'Makefile'), encoding='utf-8').read()
    names = {}
    for m in re.finditer(r'^LOCALES\+=\s+(\S+)\s*$', mk, re.M):
        src = os.path.join(dirpath, m.group(1) + '.src')
        if os.path.exists(src):
            names[m.group(1)] = src
    for m in re.finditer(r'^SAME\+=\s+(\S+)\s+(\S+)\s*$', mk, re.M):
        if m.group(1) in names:
            names[m.group(2)] = names[m.group(1)]
    return names


def text_of(src, encoding):
    """A .src file's lines without its comments, as UTF-8."""
    raw = open(src, 'rb').read()
    text = raw.decode(CODECS[encoding])
    lines = [l for l in text.split('\n')
             if not re.match(r'^(#$|# )', l)]
    return '\n'.join(lines).encode('utf-8')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--data', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()

    chars = read_unicode(os.path.join(args.data, 'unicode', 'UnicodeData.txt'))
    wide = read_widths(os.path.join(args.data, 'unicode', 'EastAsianWidth.txt'))
    os.makedirs(os.path.join(args.out, 'UTF-8'), exist_ok=True)
    with open(os.path.join(args.out, 'UTF-8', 'LC_CTYPE'), 'wb') as f:
        f.write(build_ctype(chars, wide))
    with open(os.path.join(args.out, 'UTF-8', 'LC_COLLATE'), 'wb') as f:
        f.write(build_collate())

    # language_COUNTRY -> category -> (src, encoding)
    found = {}
    for d, cat in CATEGORIES.items():
        for name, src in read_category(os.path.join(args.data, d)).items():
            base, _, enc = name.partition('.')
            if enc not in CODECS or '@' in enc:
                continue
            have = found.setdefault(base, {}).get(cat)
            if have is None or PREFER.index(enc) < PREFER.index(have[1]):
                found[base][cat] = (src, enc)

    made = []
    for base in sorted(found):
        cats = found[base]
        if len(cats) != len(CATEGORIES):
            continue
        try:
            texts = {cat: text_of(src, enc) for cat, (src, enc) in cats.items()}
        except (UnicodeDecodeError, LookupError):
            continue
        d = os.path.join(args.out, base + '.UTF-8')
        os.makedirs(d, exist_ok=True)
        for cat, text in sorted(texts.items()):
            path = os.path.join(d, cat)
            if cat == 'LC_MESSAGES':
                # Darwin's libc reads LC_MESSAGES/LC_MESSAGES, as macOS
                # lays it out.
                os.makedirs(path, exist_ok=True)
                path = os.path.join(path, cat)
            with open(path, 'wb') as f:
                f.write(text)
        for cat in ('LC_CTYPE', 'LC_COLLATE'):
            link = os.path.join(d, cat)
            if os.path.lexists(link):
                os.unlink(link)
            os.symlink(os.path.join('..', 'UTF-8', cat), link)
        made.append(base + '.UTF-8')
    print('mklocales: UTF-8 and %d locales' % len(made))
    return 0


if __name__ == '__main__':
    sys.exit(main())
