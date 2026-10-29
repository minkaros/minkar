# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# accton: FreeBSD main's usr.sbin/accton, in /usr/sbin as on macOS;
# changes for Darwin are the lines marked Minkar. It turns on acct(2),
# Darwin's process accounting, which sa(8) reads.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	accton
SRCS =	accton.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/accton
	install -m 0444 ${.CURDIR}/accton.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
