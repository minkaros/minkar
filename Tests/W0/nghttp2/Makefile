# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# nghttp2's own tests (W0): its CTest list, the munit suite (main) and the
# allocation-failure suite (failmalloc), run by run.sh. They use the
# library's internal functions, so they link nghttp2's static library,
# built from the same source with the same compiler.

SUBDIR =
EXTLIB =	nghttp2
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/nghttp2
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DENABLE_LIB_ONLY=ON -DBUILD_STATIC_LIBS=ON -DENABLE_DOC=OFF \
		-DBUILD_TESTING=ON -DENABLE_FAILMALLOC=ON
EXTLIB_TEST_BUILD = ninja -C ${BUILD} main failmalloc
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/tests/main build/tests/failmalloc

.include <minkar.extlib.mk>
