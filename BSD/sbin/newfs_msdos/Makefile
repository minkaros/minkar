# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# newfs_msdos: FreeBSD main's sbin/newfs_msdos (BSD-2, each file's
# header; commit 6a65632e1a6aeb4277769ae7cc38c076fb1a5146), in /sbin as on macOS. Changes for
# Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	newfs_msdos
SRCS =	newfs_msdos.c mkfs_msdos.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/newfs_msdos
	install -m 0444 ${.CURDIR}/newfs_msdos.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
