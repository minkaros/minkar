# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# kextstart: ends IOKit's wait for kextd at boot (see kextstart.c).

SUBDIR =
MK_WERROR = no	# the SDK headers warn under -Wsystem-headers
PROG =	kextstart
MAN =
BINDIR = /usr/libexec

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD =	-framework IOKit -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR}
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/

.include <bsd.prog.mk>
