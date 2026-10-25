# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# minkarinit: temporary PID 1 for bring-up (architecture plan, gate M1).
# Image/Makefile installs it as /sbin/launchd until launchd runs.

SUBDIR =

PROG =	minkarinit
SRCS =	minkarinit.c
MAN =

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	cp -f ${PROG} ${SYSROOT_DIR}/sbin/

.include <bsd.prog.mk>
