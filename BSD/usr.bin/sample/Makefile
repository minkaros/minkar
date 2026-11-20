# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# sample: ours, where a running process spends its time.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	sample
MAN =	sample.1
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/${PROG}
	install -C -m 0444 ${.CURDIR}/sample.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
