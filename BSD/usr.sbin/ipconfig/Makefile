# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# ipconfig: the IP configuration, as macOS's ipconfig (architecture plan, Phase 2).

SUBDIR =

PROG =	ipconfig
SRCS =	ipconfig.c
MAN =

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin
	cp -f ${PROG} ${SYSROOT_DIR}/usr/sbin/

.include <bsd.prog.mk>
