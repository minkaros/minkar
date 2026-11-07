# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libepoxy's own tests (W0): its meson test list, gathered by
# meson-collect.py, run by run.sh. Built as the system's (no GLX, no EGL),
# its tests are the ones that need no GL context, and the CGL ones, on
# Minkar's OpenGL (Mesa); the patch keeps those from needing Carbon.

SUBDIR =
EXTLIB =	libepoxy
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libepoxy
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dglx=no -Degl=no -Dx11=false -Dtests=true -Ddocs=false
EXTLIB_PATCHES = 0001-tests-cgl-without-carbon.patch
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/../runner/meson-collect.py ${BUILD} ${SRC} ${.CURDIR}
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
