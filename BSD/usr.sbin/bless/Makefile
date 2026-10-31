# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# bless: ours, a volume.s boot information and the boot device
# (HFS+ Finder info, NVRAM efi-boot-device), in /usr/sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	bless
SRCS =	bless.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/bless
	install -m 0444 ${.CURDIR}/bless.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
