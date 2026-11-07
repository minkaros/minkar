# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxml2's own tests (W0): the programs its CMake test list runs, with
# the source tree's test data and expected results, run by run.sh.

SUBDIR =
EXTLIB =	libxml2
EXTLIB_TEST =	yes
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/Frameworks/CoreText/libxml2
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_ICU=OFF -DLIBXML2_WITH_PYTHON=OFF \
		-DLIBXML2_WITH_TESTS=ON -DLIBXML2_WITH_PROGRAMS=ON -DLIBXML2_WITH_SAX1=ON
EXTLIB_TEST_FILES = build/runtest build/runsuite build/testapi build/testchar build/testdict \
		build/testparser build/testrecurse build/testThreads build/xmllint \
		src/test src/result src/xstc

.include <minkar.extlib.mk>
