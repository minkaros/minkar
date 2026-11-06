# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxkbcommon, keymaps for WebKit's WPE platform (W0), reading the
# keyboard descriptions in /usr/share/X11/xkb (xkeyboard-config). Without
# X11 or Wayland.

SUBDIR =
EXTLIB =	libxkbcommon
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Denable-x11=false -Denable-wayland=false -Denable-docs=false \
		-Denable-tools=false -Denable-xkbregistry=false -Dxkb-config-root=/usr/share/X11/xkb
EXTLIB_LIBS =	libxkbcommon.0.dylib
EXTLIB_LICENSES = LICENSE

.include <minkar.extlib.mk>
