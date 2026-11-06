# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Libgcrypt, the cryptography WebKit's Web Crypto API uses (W0), on
# libgpg-error.

SUBDIR =
EXTLIB =	libgcrypt
EXTLIB_SYSTEM =	autotools
# libgpg-error's gpgrt-config, in the SDK (it finds gpg-error.pc beside it).
EXTLIB_ENV =	GPGRT_CONFIG=${SDK}/usr/bin/gpgrt-config
# The C versions of the multi-precision routines (their assembly is not
# assembled for this target).
EXTLIB_OPTIONS = --disable-doc --disable-asm
EXTLIB_STAGE_CMD = gmake -C ${BUILD}/src DESTDIR=${STAGE} install
EXTLIB_LIBS =	libgcrypt.20.dylib
EXTLIB_LICENSES = COPYING.LIB

.include <minkar.extlib.mk>
