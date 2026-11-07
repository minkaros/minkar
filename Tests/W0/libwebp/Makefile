# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libwebp's own tests (W0): its source has no unit tests (only fuzzers),
# so its test vectors (libwebp-test-data, pinned in
# BSD/lib/libwebp/MANIFEST.libwebp-test-data) with their scripts:
# test_dwebp.sh (every vector decoded to five formats, with and without
# SIMD and threads, against MD5 sums) and test_lossless.sh (lossless
# vectors against reference images), run by run.sh with dwebp.

SUBDIR =
EXTLIB =	libwebp
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libwebp
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=ON \
		-DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF \
		-DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
EXTLIB_SUBPROJECTS = libwebp-test-data
EXTLIB_SUBPROJECTS_DIR = testdata
EXTLIB_TEST_BUILD = rm -rf ${SRC}/testdata/libwebp-test-data/.git
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/dwebp src/testdata/libwebp-test-data

.include <minkar.extlib.mk>
