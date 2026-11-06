# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# FreeType 2.13.2 (FTL; docs/FTL.TXT), the copy Onyx2D builds statically
# (Frameworks/Onyx2D/freetype), built from it as /usr/lib/libfreetype.6.dylib
# for fontconfig, HarfBuzz and WebKit (W0), on zlib, libpng and Brotli
# (WOFF2); without HarfBuzz, which is built on it.

SUBDIR =
EXTLIB =	freetype
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/Frameworks/Onyx2D/freetype
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dharfbuzz=disabled -Dpng=enabled -Dbrotli=enabled -Dzlib=system -Dbzip2=disabled \
		-Dtests=disabled
EXTLIB_LIBS =	libfreetype.6.dylib
EXTLIB_LICENSES = LICENSE.TXT docs/FTL.TXT

.include <minkar.extlib.mk>
