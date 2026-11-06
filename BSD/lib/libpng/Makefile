# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libpng 1.6.43 (libpng license, LICENSE), the copy in this directory:
# Onyx2D builds it statically from here (Frameworks/Onyx2D/Makefile), and
# this Makefile builds it as /usr/lib/libpng16.16.dylib for WebKit (W0).

SUBDIR =
EXTLIB =	libpng
EXTLIB_SOURCE =	${.CURDIR}
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DPNG_STATIC=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF
EXTLIB_LIBS =	libpng16.16.dylib
EXTLIB_LICENSES = LICENSE

.include <minkar.extlib.mk>
