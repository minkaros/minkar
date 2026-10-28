# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# compress: FreeBSD main's usr.bin/compress, unchanged.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	compress
SRCS =	compress.c zopen.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/compress
	install -m 0444 ${.CURDIR}/compress.1 ${SYSROOT_DIR}/usr/share/man/man1/
	ln -f ${SYSROOT_DIR}/usr/bin/compress ${SYSROOT_DIR}/usr/bin/uncompress
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/compress.1 ${SYSROOT_DIR}/usr/share/man/man1/uncompress.1

.include <bsd.prog.mk>
