# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# dav1d, the AV1 decoder (AU4), for FFmpeg: the library, with its x86
# assembly (nasm), without its tools and tests. In /usr/lib/media, with
# the media frameworks' other decoders.

SUBDIR =
EXTLIB =	dav1d
EXTLIB_SYSTEM =	meson
EXTLIB_PRIVATE = media
EXTLIB_OPTIONS = -Denable_tools=false -Denable_tests=false -Denable_examples=false \
		-Denable_asm=true -Dbitdepths=8,16
EXTLIB_LIBS =	libdav1d.7.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
