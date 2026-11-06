# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Expat's own tests (W0): runtests and runtests_cxx, on the system's
# libexpat.

SUBDIR =
EXTLIB =	expat
EXTLIB_TEST =	yes
EXTLIB_MANIFEST = ${ROOT_SOURCE_DIR}/BSD/lib/expat/MANIFEST
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=ON \
		-DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=ON
EXTLIB_TEST_FILES = build/tests/runtests build/tests/runtests_cxx

.include <minkar.extlib.mk>
