# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libffi's own tests (W0): every program of its DejaGnu suite, built by
# collect.py the way its .exp files build them (clang: -W -Wall, at -O0
# and -O2), with the output each must print; run.sh runs them.

SUBDIR =
EXTLIB =	libffi
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libffi
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --disable-docs --disable-multi-os-directory
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/collect.py ${BUILD} ${SRC} ${.CURDIR}/dgmatch.c
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
