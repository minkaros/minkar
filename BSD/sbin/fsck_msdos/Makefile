# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fsck_msdos: FreeBSD main's sbin/fsck_msdosfs (BSD-2/BSD-4 licences,
# each file's header; commit 6a65632e1a6aeb4277769ae7cc38c076fb1a5146), with fsutil.c and fsutil.h
# from its sbin/fsck, installed under macOS's name /sbin/fsck_msdos.
# Changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	fsck_msdos
SRCS =	main.c check.c boot.c fat.c dir.c fsutil.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} -DHAVE_LIBUTIL_H \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/fsck_msdos
	install -m 0444 ${.CURDIR}/fsck_msdos.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
