# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Fontconfig 2.14 (MIT; COPYING), the copy CoreText builds statically
# (Frameworks/CoreText/fontconfig), built from it as
# /usr/lib/libfontconfig.1.dylib for WebKit (W0), configured as CoreText's
# is (/etc/fonts, its caches in /var, its templates in CoreText's
# resources, the system's font folders), on FreeType and Expat.

SUBDIR =
EXTLIB =	fontconfig
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/Frameworks/CoreText/fontconfig
EXTLIB_SYSTEM =	autotools
EXTLIB_ENV =	GPERF="python3 -I ${ROOT_SOURCE_DIR}/Frameworks/CoreText/fcobjshash-gen.py"
EXTLIB_OPTIONS = --sysconfdir=/etc --localstatedir=/var --disable-docs --disable-nls \
		--disable-cache-build \
		--with-templatedir=/System/Library/Frameworks/CoreText.framework/Resources \
		--with-xmldir=/System/Library/Frameworks/CoreText.framework/Resources \
		--with-default-fonts=/System/Library/Fonts,/Library/Fonts
EXTLIB_TARGETS = -C src all
EXTLIB_STAGE_CMD = gmake -C ${BUILD}/src DESTDIR=${STAGE} install && \
		gmake -C ${BUILD}/fontconfig DESTDIR=${STAGE} install && \
		gmake -C ${BUILD} DESTDIR=${STAGE} install-pkgconfigDATA
EXTLIB_LIBS =	libfontconfig.1.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
