# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fs_usage: ours, file system and network activity from the
# kernel.s trace (kdebug), in /usr/bin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	fs_usage
SRCS =	fs_usage.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${MINKAR_SDKROOT}/usr/local/include
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/fs_usage
	install -m 0444 ${.CURDIR}/fs_usage.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
