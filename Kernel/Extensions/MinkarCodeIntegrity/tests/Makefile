# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# cstest: the code integrity policy's test program. One copy in the trust
# cache, three outside it (see cstest.c). Built in the CoreServices stage,
# after IOKit; mktrustcache leaves OUTSIDE out of the cache (Image/Makefile).

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	cstest
SRCS =	cstest.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework IOKit -framework CoreFoundation

CODESIGN = python3 -B ${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/codesign/codesign.py
OUTSIDE = /usr/local/tests/codesign/outside

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin ${SYSROOT_DIR}${OUTSIDE}
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/cstest
	${CODESIGN} -f -s - -i org.minkar.cstest \
	    --entitlements ${.CURDIR}/restricted.entitlements \
	    ${SYSROOT_DIR}/usr/local/bin/cstest
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${OUTSIDE}/cstest-resigned
	${CODESIGN} -f -s - -i org.minkar.cstest-resigned \
	    --entitlements ${.CURDIR}/restricted.entitlements \
	    ${SYSROOT_DIR}${OUTSIDE}/cstest-resigned
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${OUTSIDE}/cstest-adhoc
	${CODESIGN} -f -s - -i org.minkar.cstest-adhoc \
	    --entitlements ${.CURDIR}/unrestricted.entitlements \
	    ${SYSROOT_DIR}${OUTSIDE}/cstest-adhoc
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${OUTSIDE}/cstest-unsigned

.include <bsd.prog.mk>
