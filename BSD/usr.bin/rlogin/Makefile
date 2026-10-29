# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# rlogin: NetBSD's usr.bin/rlogin with getport.c from its usr.bin/rsh
# (FreeBSD no longer has rlogin), in /usr/bin as on macOS: setuid root,
# for the reserved port rcmd(3) binds. rlogin_darwin.h gives two NetBSD
# names Darwin lacks; changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	rlogin
SRCS =	rlogin.c getport.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} -include rlogin_darwin.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 4555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/rlogin
	install -m 0444 ${.CURDIR}/rlogin.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
