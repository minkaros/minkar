# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fdisk: FreeBSD main's sbin/fdisk (MBR partitions), in /usr/sbin as on
# macOS; changes for Darwin are the lines marked Minkar. include/ gives it
# FreeBSD's <sys/diskmbr.h> and <sys/disk/mbr.h>, and our <libgeom.h> and
# <sys/disklabel.h>: the few libgeom calls over Darwin's disk ioctls, and
# no GEOM.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	fdisk
SRCS =	fdisk.c fdisk_mbr_enc.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR}/include \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/fdisk
	install -m 0444 ${.CURDIR}/fdisk.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
