# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# GNU Libtasn1, ASN.1 for WebKit's Web Crypto API (W0).

SUBDIR =
EXTLIB =	libtasn1
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --disable-doc --disable-valgrind-tests
EXTLIB_TARGETS = -C lib
EXTLIB_STAGE_CMD = gmake -C ${BUILD}/lib DESTDIR=${STAGE} install
EXTLIB_LIBS =	libtasn1.6.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
