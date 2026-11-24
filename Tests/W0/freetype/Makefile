# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# FreeType's tests (W0): Minkar's own (ft-test.c), as FreeType's source has none that
# runs without a download; run.sh runs them on the system's OFL fonts.

SUBDIR =
SDK =		${MINKAR_SDKROOT}
TEST_DIR =	${SYSROOT_DIR}/usr/local/tests/w0/freetype

all: install-test

ft-test: ${.CURDIR}/ft-test.c
	${CC} --sysroot=${SDK} -mmacos-version-min=${MACOS_VERSION_MIN} --ld-path=${LD} -O2 \
	    -I${SDK}/usr/local/webkit/include/freetype2 -L${SDK}/usr/lib/webkit -o ${.TARGET} ${.CURDIR}/ft-test.c -lfreetype

install-test: ft-test .PHONY
	mkdir -p ${TEST_DIR}
	install -C -m 0755 ft-test ${TEST_DIR}/ft-test
	install -C -m 0755 ${.CURDIR}/run.sh ${TEST_DIR}/run.sh

.include <bsd.prog.mk>
