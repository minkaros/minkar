# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# GNU Libtasn1's own tests (W0): its test programs, with their data.

SUBDIR =
EXTLIB =	libtasn1
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libtasn1
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --disable-doc --disable-valgrind-tests
EXTLIB_TEST_BUILD = gmake -C ${BUILD}/tests check TESTS=
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/tests/.libs src/tests

.include <minkar.extlib.mk>
