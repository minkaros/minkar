# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Expat, the XML parser fontconfig reads its configuration with (WebKit W0).

SUBDIR =
EXTLIB =	expat
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=OFF \
		-DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=ON
EXTLIB_LIBS =	libexpat.1.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
