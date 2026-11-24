# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# WOFF2's tests (W0): Minkar's own (woff2-test.cc), as WOFF2's source has none that
# runs without a download; run.sh runs them on the system's OFL fonts.

SUBDIR =
SDK =		${MINKAR_SDKROOT}
TEST_DIR =	${SYSROOT_DIR}/usr/local/tests/w0/woff2

all: install-test

woff2-test: ${.CURDIR}/woff2-test.cc
	${CXX} -std=c++17 --sysroot=${SDK} -mmacos-version-min=${MACOS_VERSION_MIN} --ld-path=${LD} -O2 \
	    -I${SDK}/usr/local/webkit/include -L${SDK}/usr/lib/webkit -o ${.TARGET} ${.CURDIR}/woff2-test.cc -lwoff2enc -lwoff2dec -lwoff2common

install-test: woff2-test .PHONY
	mkdir -p ${TEST_DIR}
	install -C -m 0755 woff2-test ${TEST_DIR}/woff2-test
	install -C -m 0755 ${.CURDIR}/run.sh ${TEST_DIR}/run.sh

.include <bsd.prog.mk>
