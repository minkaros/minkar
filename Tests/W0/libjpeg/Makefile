# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libjpeg-turbo's own tests (W0): its CTest list, gathered by collect.py
# into w0/ with the programs and test images, run by run.sh.

SUBDIR =
EXTLIB =	libjpeg
EXTLIB_TEST =	yes
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/BSD/lib/libjpeg
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DENABLE_STATIC=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=ON \
		-DWITH_TESTS=ON
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/collect.py ${BUILD} ${SRC}
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
