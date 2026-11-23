# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# opendiff: two files' differences in Text Editor (a unified diff), or the two
# merged with their conflicts marked; there is no comparison app yet.

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
MAN =
PROG =	opendiff
SRCS =	opendiff.c
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -C -m 0444 ${.CURDIR}/opendiff.1 ${SYSROOT_DIR}/usr/share/man/man1/opendiff.1

.include <bsd.prog.mk>
