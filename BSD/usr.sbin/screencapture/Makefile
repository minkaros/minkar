# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# screencapture, on the window server's capture (CoreGraphics) and its
# image encoders (Phase 4).

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
MAN =
PROG =	screencapture
SRCS =	screencapture.m
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =	-framework AppKit -framework CoreGraphics -framework Foundation \
	-framework CoreFoundation -lobjc

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 ${PROG} ${SYSROOT_DIR}/usr/sbin/
	install -C -m 0444 ${.CURDIR}/screencapture.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
