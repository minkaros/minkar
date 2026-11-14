# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# afplay: plays a sound file through AudioToolbox (AU3).

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
MAN =
PROG =	afplay
SRCS =	afplay.c
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =	-framework AudioToolbox -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -C -m 0444 ${.CURDIR}/afplay.1 ${SYSROOT_DIR}/usr/share/man/man1/afplay.1

.include <bsd.prog.mk>
