# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libgpg-error's own tests (W0): its t-* programs.

SUBDIR =
EXTLIB =	libgpg-error
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libgpg-error
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --disable-doc --disable-nls --disable-languages --host=x86_64-apple-darwin
EXTLIB_TEST_BUILD = gmake -C ${BUILD}/tests check TESTS=
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/tests/.libs

.include <minkar.extlib.mk>
