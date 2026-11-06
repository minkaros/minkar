# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# HarfBuzz's own tests (W0): its meson tests, gathered by collect.py into
# w0/ with their programs and data, run by run.sh. Its two Python runners
# are replaced by shape.sh and fuzz.sh; the subset tests need Python and
# fontTools, which Minkar has not.

SUBDIR =
EXTLIB =	harfbuzz
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/harfbuzz
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dfreetype=enabled -Dglib=enabled -Dicu=enabled -Dgobject=disabled -Dcairo=disabled \
		-Dcoretext=disabled -Dchafa=disabled -Dgraphite2=disabled -Dintrospection=disabled \
		-Ddocs=disabled -Dtests=enabled -Dbenchmark=disabled -Dutilities=enabled
EXTLIB_TEST_BUILD = python3 ${.CURDIR}/collect.py ${BUILD} ${SRC} ${.CURDIR}
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
