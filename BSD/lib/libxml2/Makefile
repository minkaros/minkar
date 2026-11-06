# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxml2 2.12.4 (MIT; Copyright), the copy CoreText builds statically
# (Frameworks/CoreText/libxml2), built from it as /usr/lib/libxml2.2.dylib,
# as the reference platform has it, for libxslt and WebKit (W0): one
# libxml2 shared by the libraries that pass its documents to each other.
# On zlib and iconv, without lzma, ICU or Python.

SUBDIR =
EXTLIB =	libxml2
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/Frameworks/CoreText/libxml2
EXTLIB_PUBLIC =	yes
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_ICU=OFF -DLIBXML2_WITH_PYTHON=OFF \
		-DLIBXML2_WITH_TESTS=OFF -DLIBXML2_WITH_PROGRAMS=OFF -DLIBXML2_WITH_SAX1=ON
EXTLIB_LIBS =	libxml2.2.dylib
EXTLIB_LICENSES = Copyright

.include <minkar.extlib.mk>
