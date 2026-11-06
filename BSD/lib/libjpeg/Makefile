# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libjpeg-turbo 3.0.2 (IJG, BSD-3-Clause and zlib; LICENSE.md), the copy in
# this directory: Onyx2D builds it statically from here, and this Makefile
# builds its libjpeg API as /usr/lib/libjpeg.62.dylib for WebKit (W0),
# without the SIMD code (it needs NASM on the build host).

SUBDIR =
EXTLIB =	libjpeg
EXTLIB_SOURCE =	${.CURDIR}
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DENABLE_STATIC=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF \
		-DWITH_TESTS=OFF
EXTLIB_LIBS =	libjpeg.62.dylib
EXTLIB_LICENSES = LICENSE.md

.include <minkar.extlib.mk>
