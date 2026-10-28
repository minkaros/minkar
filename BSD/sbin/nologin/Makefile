# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# nologin: FreeBSD main's usr.sbin/nologin, in /sbin as on macOS; changes for Darwin
# are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	nologin
SRCS =	nologin.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man5 ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/nologin
	install -m 0444 ${.CURDIR}/nologin.5 ${SYSROOT_DIR}/usr/share/man/man5/
	install -m 0444 ${.CURDIR}/nologin.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
