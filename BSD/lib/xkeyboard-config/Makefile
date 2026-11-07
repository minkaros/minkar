# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# xkeyboard-config, the keyboard descriptions libxkbcommon reads, in
# /usr/share/X11/xkb (WebKit W0). Data only.

SUBDIR =
EXTLIB =	xkeyboard-config
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dxorg-rules-symlinks=false -Dcompat-rules=true -Dnls=false
EXTLIB_DATA =	usr/share/X11/xkb
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
