# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# dot_clean: our own, in /usr/sbin as on macOS, over copyfile(3)'s
# AppleDouble unpacking.

SUBDIR =
WARNS ?= 3
PROG =	dot_clean
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}


all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/dot_clean
	install -m 0444 ${.CURDIR}/dot_clean.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
