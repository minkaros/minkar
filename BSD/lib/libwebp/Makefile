# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libwebp, WebP images for WebKit (W0): the libraries, with demux and mux.

SUBDIR =
EXTLIB =	libwebp
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
		-DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF \
		-DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
EXTLIB_LIBS =	libwebp.7.dylib libwebpdemux.2.dylib libwebpmux.3.dylib libsharpyuv.0.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
