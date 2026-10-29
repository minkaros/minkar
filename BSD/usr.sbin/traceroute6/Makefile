# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# traceroute6: FreeBSD main's usr.sbin/traceroute6, in /usr/sbin as on macOS, through the
# routing socket (FreeBSD's WITHOUT_NETLINK path); changes for Darwin are
# the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	traceroute6
SRCS =	traceroute6.c as.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} -DHAVE_POLL -D__APPLE_USE_RFC_3542 \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat 

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 4555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/traceroute6
	install -m 0444 ${.CURDIR}/traceroute6.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
