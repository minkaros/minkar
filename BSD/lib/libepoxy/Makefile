# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libepoxy, the OpenGL function loader WebKit requires (W0), with EGL:
# Mesa's libEGL and libGLESv2 (../mesa). No GLX.

SUBDIR =
EXTLIB =	libepoxy
EXTLIB_SYSTEM =	meson
EXTLIB_PATCHES = 0001-minkar-egl.patch
EXTLIB_OPTIONS = -Dglx=no -Degl=yes -Dx11=false -Dtests=false -Ddocs=false
EXTLIB_LIBS =	libepoxy.0.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
