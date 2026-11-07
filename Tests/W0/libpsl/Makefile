# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libpsl's own tests (W0): its meson tests, gathered by
# runner/meson-collect.py into w0/ with the Public Suffix List and the
# DAFSA files built from it, run by run.sh. A test-only patch gives the
# tests their data's directory on Minkar.

SUBDIR =
EXTLIB =	libpsl
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libpsl
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_PATCHES = 0001-tests-data-dir-option.patch
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Druntime=no -Dbuiltin=true -Dtests=true -Ddocs=false \
		-Dtests_data_dir=/usr/local/tests/w0/libpsl/w0
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/../runner/meson-collect.py ${BUILD} ${SRC} ${.CURDIR} \
		tests list fuzz build:tests/psl.dafsa build:tests/psl_ascii.dafsa
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
