# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libpng's own tests (W0): its CTest list, gathered by collect.py into w0/
# with the programs and PNG files, run by run.sh.

SUBDIR =
EXTLIB =	libpng
EXTLIB_TEST =	yes
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/BSD/lib/libpng
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DPNG_STATIC=OFF -DPNG_TESTS=ON -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/collect.py ${BUILD} ${SRC}
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
