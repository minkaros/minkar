# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# PCRE2's own tests (W0): RunTest, its script, runs pcre2test over the
# test data and compares the output with the expected one, with and
# without the JIT.

SUBDIR =
EXTLIB =	pcre2
EXTLIB_TEST =	yes
EXTLIB_MANIFEST = ${ROOT_SOURCE_DIR}/BSD/lib/pcre2/MANIFEST
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_BUILD_PCRE2_16=OFF -DPCRE2_BUILD_PCRE2_32=OFF \
		-DPCRE2_SUPPORT_JIT=ON -DPCRE2_BUILD_PCRE2GREP=ON -DPCRE2_BUILD_TESTS=ON \
		-DPCRE2_SUPPORT_LIBZ=OFF -DPCRE2_SUPPORT_LIBBZ2=OFF -DPCRE2_SUPPORT_LIBREADLINE=OFF \
		-DPCRE2_SUPPORT_LIBEDIT=OFF -DBUILD_STATIC_LIBS=OFF
EXTLIB_TEST_FILES = build/pcre2test build/pcre2grep src/RunTest src/RunGrepTest src/testdata

.include <minkar.extlib.mk>
