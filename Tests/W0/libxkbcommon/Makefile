# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxkbcommon's own tests (W0): its meson tests, gathered by
# runner/meson-collect.py into w0/ with their programs and the source
# tree's test data, run by run.sh.

SUBDIR =
EXTLIB =	libxkbcommon
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libxkbcommon
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_PATCHES = 0001-tests-test-data-dir-option.patch
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Denable-x11=false -Denable-wayland=false -Denable-docs=false \
		-Denable-tools=false -Denable-xkbregistry=false -Dxkb-config-root=/usr/share/X11/xkb \
		-Dtest-data-dir=/usr/local/tests/w0/libxkbcommon/w0/src/test/data
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/../runner/meson-collect.py ${BUILD} ${SRC} ${.CURDIR} test
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
