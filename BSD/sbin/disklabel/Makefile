# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# disklabel: ours, Apple labels (a header and a property list) on
# a partition, in /sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	disklabel
SRCS =	disklabel.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -lz

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/disklabel
	install -m 0444 ${.CURDIR}/disklabel.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
