# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# rtsol: FreeBSD main's usr.sbin/rtsold (KAME's), installed as /sbin/rtsol
# as on macOS, where it sends IPv6 router solicitations once and applies
# the answers (rtsold under another name); without casper. rtsol_darwin.h
# gives Darwin's names for what FreeBSD has. Changes for
# Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	rtsol
SRCS =	cap_llflags.c cap_script.c cap_sendmsg.c dump.c if.c rtsock.c \
	rtsol.c rtsold.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} -include rtsol_darwin.h \
	-D__APPLE_USE_RFC_3542 \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/rtsol
	install -m 0444 ${.CURDIR}/rtsold.8 ${SYSROOT_DIR}/usr/share/man/man8/
	ln -sf rtsold.8 ${SYSROOT_DIR}/usr/share/man/man8/rtsol.8

.include <bsd.prog.mk>
