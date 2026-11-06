# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libepoxy, the OpenGL function loader WebKit requires (W0). Without GLX
# and EGL for now: Minkar's Mesa has its Vulkan driver only.

SUBDIR =
EXTLIB =	libepoxy
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dglx=no -Degl=no -Dx11=false -Dtests=false -Ddocs=false
EXTLIB_LIBS =	libepoxy.0.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
