# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Libgcrypt's own tests (W0): its test programs, but the benchmarks and
# the hash tests over 6 and 256 GB (run.sh).

SUBDIR =
EXTLIB =	libgcrypt
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libgcrypt
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	autotools
EXTLIB_ENV =	GPGRT_CONFIG=${SDK}/usr/local/webkit/bin/gpgrt-config
EXTLIB_OPTIONS = --disable-doc --disable-asm
EXTLIB_TEST_BUILD = gmake -C ${BUILD}/tests check TESTS=
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/tests/.libs src/tests

.include <minkar.extlib.mk>
