# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# leaks: ours, the malloc blocks a process can no longer reach.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	leaks
MAN =	leaks.1
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/${PROG}
	install -m 0444 ${.CURDIR}/leaks.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
