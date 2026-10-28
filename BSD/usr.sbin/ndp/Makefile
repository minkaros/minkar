# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ndp: FreeBSD main's usr.sbin/ndp, in /usr/sbin as on macOS, through the
# routing socket (FreeBSD's WITHOUT_NETLINK path); changes for Darwin are
# the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	ndp
SRCS =	ndp.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} -DWITHOUT_NETLINK -D_U_= \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lxo

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/ndp
	install -m 0444 ${.CURDIR}/ndp.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
