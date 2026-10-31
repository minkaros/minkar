# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mount_exfat: ours, mounting exFAT file systems with the exfat kext, in
# /sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 3
PROG =	mount_exfat
SRCS =	mount_exfat.c
MAN =
LIBUTIL =	${ROOT_SOURCE_DIR}/BSD/lib/libutil
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${LIBUTIL} \
	-I${ROOT_SOURCE_DIR}/Kernel/Extensions/exfat
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${LIBUTIL} -lutil

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/mount_exfat
	install -m 0444 ${.CURDIR}/mount_exfat.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
