#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

"""
system-colors.py: Atlas's system colors (light appearance), the "System"
color list AppKit's named colors come from (+[NSColor controlColor] and
the others), written as System.clr: an NSKeyedArchiver property list of
the names and, for each, a catalog color holding its value.

    system-colors.py [--check]

--check compares System.clr with the table (exit status 1 if it differs).
Atlas's own palette: a light grey window, white controls and content,
one blue accent; WhiteSur served as a reference for the proportions.
"""

import os
import plistlib
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'System.clr')

ACCENT = (0.13, 0.47, 0.95)
SELECTION = (0.70, 0.84, 1.00)

# name: grey (one number) or RGB (three), then alpha (optional).
COLORS = [
    # Text.
    ('labelColor', 0.0),
    ('secondaryLabelColor', 0.40),
    ('tertiaryLabelColor', 0.60),
    ('quaternaryLabelColor', 0.75),
    ('textColor', 0.0),
    ('textBackgroundColor', 1.0),
    ('selectedTextColor', 0.0),
    ('selectedTextBackgroundColor', SELECTION),
    ('unemphasizedSelectedTextColor', 0.0),
    ('unemphasizedSelectedTextBackgroundColor', 0.86),
    ('placeholderTextColor', 0.60),
    ('linkColor', ACCENT),
    ('headerTextColor', 0.0),
    ('findHighlightColor', (1.0, 0.85, 0.20)),
    # Windows and content.
    ('windowBackgroundColor', 0.93),
    ('underPageBackgroundColor', 0.85),
    ('windowFrameColor', 0.85),
    ('windowFrameTextColor', 0.0),
    ('controlBackgroundColor', 1.0),
    ('controlAlternatingRowColor', 0.96),
    ('alternatingContentBackgroundColor', 0.96),
    ('selectedContentBackgroundColor', ACCENT),
    ('unemphasizedSelectedContentBackgroundColor', 0.86),
    ('gridColor', 0.85),
    ('separatorColor', 0.80),
    # Controls.
    ('controlColor', 1.0),
    ('controlTextColor', 0.0),
    ('disabledControlTextColor', 0.60),
    ('selectedControlColor', SELECTION),
    ('selectedControlTextColor', 0.0),
    ('alternateSelectedControlColor', ACCENT),
    ('alternateSelectedControlTextColor', 1.0),
    ('secondarySelectedControlColor', 0.86),
    ('controlAccentColor', ACCENT),
    ('keyboardFocusIndicatorColor', ACCENT, 0.5),
    ('controlHighlightColor', 0.97),
    ('controlLightHighlightColor', 1.0),
    ('controlShadowColor', 0.55),
    ('controlDarkShadowColor', 0.30),
    ('highlightColor', 1.0),
    ('shadowColor', 0.0),
    ('scrollBarColor', 0.93),
    ('knobColor', 0.70),
    ('selectedKnobColor', 0.55),
    # Menus.
    ('menuBackgroundColor', 0.97),
    ('menuItemTextColor', 0.0),
    ('selectedMenuItemColor', ACCENT),
    ('selectedMenuItemTextColor', 1.0),
    # The system's colors.
    ('systemRedColor', (0.93, 0.26, 0.24)),
    ('systemOrangeColor', (1.0, 0.58, 0.0)),
    ('systemYellowColor', (1.0, 0.80, 0.0)),
    ('systemGreenColor', (0.20, 0.74, 0.32)),
    ('systemTealColor', (0.25, 0.70, 0.80)),
    ('systemBlueColor', ACCENT),
    ('systemIndigoColor', (0.35, 0.34, 0.84)),
    ('systemPurpleColor', (0.69, 0.32, 0.87)),
    ('systemPinkColor', (0.98, 0.26, 0.48)),
    ('systemBrownColor', (0.64, 0.52, 0.37)),
    ('systemGrayColor', 0.56),
    ('systemLightGrayColor', 0.75),
    ('systemDarkGrayColor', 0.33),
]


def archive():
    objects = ['$null']

    def add(obj):
        objects.append(obj)
        return {'CF$UID': len(objects) - 1}

    def cls(name, chain):
        return add({'$classname': name, '$classes': chain})

    array_class = cls('NSMutableArray', ['NSMutableArray', 'NSArray', 'NSObject'])
    color_class = cls('NSColor', ['NSColor', 'NSObject'])
    catalog = add('System')
    names, colors = [], []
    for entry in COLORS:
        name, value = entry[0], entry[1]
        alpha = entry[2] if len(entry) > 2 else 1.0
        if isinstance(value, tuple):
            inner = {'$class': color_class, 'NSColorSpace': 1,
                     'NSRGB': ('%f %f %f %f' % (value + (alpha,))).encode()}
        else:
            inner = {'$class': color_class, 'NSColorSpace': 3,
                     'NSWhite': ('%f %f' % (value, alpha)).encode()}
        key = add(name)
        names.append(key)
        colors.append(add({'$class': color_class, 'NSColorSpace': 6, 'NSCatalogName': catalog,
                           'NSColorName': key, 'NSColor': add(inner)}))
    keys = add({'$class': array_class, 'NS.objects': names})
    values = add({'$class': array_class, 'NS.objects': colors})
    return plistlib.dumps({'$archiver': 'NSKeyedArchiver', '$version': 100000,
                           '$top': {'NSKeys': keys, 'NSColors': values}, '$objects': objects},
                          fmt=plistlib.FMT_XML, sort_keys=True)


def main(argv):
    data = archive()
    if '--check' in argv:
        same = os.path.exists(OUT) and open(OUT, 'rb').read() == data
        print('system-colors: %s' % ('matches the table' if same else 'differs from the table'))
        return 0 if same else 1
    open(OUT, 'wb').write(data)
    print('system-colors: wrote System.clr (%d colors)' % len(COLORS))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
