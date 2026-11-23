# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# osascript: our own, in /usr/bin as on the reference platform, with our
# own interpreter of AppleScript's commonly used part (the maintainer's
# choice, 2026-11-23).

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
MAN =
PROG =	osascript
SRCS =	osascript.m
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot -fobjc-arc -fobjc-exceptions
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =	-framework AppKit -framework Foundation -framework CoreAudio -framework Security -lobjc

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/osascript
	install -C -m 0444 ${.CURDIR}/osascript.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
