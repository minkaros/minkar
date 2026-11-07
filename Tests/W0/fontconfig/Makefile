# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fontconfig's own tests (W0): built with its tools by 'make check', laid
# out by collect.sh as its test scripts expect, run by run.sh.

SUBDIR =
EXTLIB =	fontconfig
EXTLIB_TEST =	yes
EXTLIB_SOURCE =	${ROOT_SOURCE_DIR}/Frameworks/CoreText/fontconfig
EXTLIB_SYSTEM =	autotools
EXTLIB_ENV =	GPERF="python3 -I ${ROOT_SOURCE_DIR}/Frameworks/CoreText/fcobjshash-gen.py"
EXTLIB_OPTIONS = --sysconfdir=/etc --localstatedir=/var --disable-docs --disable-nls \
		--disable-cache-build \
		--with-templatedir=/System/Library/Frameworks/CoreText.framework/Resources \
		--with-xmldir=/System/Library/Frameworks/CoreText.framework/Resources \
		--with-default-fonts=/System/Library/Fonts,/Library/Fonts
# Two tests have their data's directory built in: where it is on Minkar.
W0_TEST =	/usr/local/tests/w0/fontconfig/w0
EXTLIB_TEST_BUILD = rm -f ${BUILD}/test/test_bz89617-* ${BUILD}/test/test_bz106632-* && \
		gmake -C ${BUILD}/test check TESTS= abs_srcdir=${W0_TEST}/test abs_top_srcdir=${W0_TEST} && \
		sh ${.CURDIR}/collect.sh ${BUILD} ${SRC}
EXTLIB_STAGE_CMD = true
EXTLIB_TEST_FILES = build/w0

.include <minkar.extlib.mk>
