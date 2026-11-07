# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# nghttp2, HTTP/2 for libsoup (WebKit W0): the library only.

SUBDIR =
EXTLIB =	nghttp2
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DENABLE_LIB_ONLY=ON -DBUILD_STATIC_LIBS=OFF -DENABLE_DOC=OFF -DBUILD_TESTING=OFF
EXTLIB_LIBS =	libnghttp2.14.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
