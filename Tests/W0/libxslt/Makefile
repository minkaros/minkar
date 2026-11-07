# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxslt's own tests (W0): runtest over the source tree's tests, with
# plugins off as in the system's, as its 'make check' runs it (run.sh).

SUBDIR =
EXTLIB =	libxslt
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libxslt
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --without-python --without-crypto --without-debugger --without-profiler
EXTLIB_TEST_BUILD = gmake -C ${BUILD}/tests runtest
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/tests/.libs/runtest src/tests

.include <minkar.extlib.mk>
