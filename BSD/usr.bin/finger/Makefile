# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# finger: FreeBSD main's usr.bin/finger, in /usr/bin as on macOS; changes for Darwin
# are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	finger
SRCS =	finger.c lprint.c net.c sprint.c util.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 ${SYSROOT_DIR}/usr/share/man/man5
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/finger
	install -m 0444 ${.CURDIR}/finger.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${.CURDIR}/finger.conf.5 ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>
