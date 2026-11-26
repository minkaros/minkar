# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# say: speaks text through eSpeak NG (/usr/libexec/espeak-ng, a process of
# its own), played with afplay or kept as AIFF or WAV.

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
MAN =
PROG =	say
SRCS =	say.c
CFLAGS = -O2 --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =	-framework AudioToolbox -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -C -m 0444 ${.CURDIR}/say.1 ${SYSROOT_DIR}/usr/share/man/man1/say.1

.include <bsd.prog.mk>
