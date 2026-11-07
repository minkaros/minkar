# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Brotli's own tests (W0): its CTest list, the brotli tool's round trips
# and the decompression of its compressed test files, run by run.sh (its
# tests run through CMake scripts, and Minkar has no CMake). The tool uses
# the system's libraries.

SUBDIR =
EXTLIB =	brotli
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/brotli
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DBROTLI_DISABLE_TESTS=ON
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/brotli src/c src/tests

.include <minkar.extlib.mk>
