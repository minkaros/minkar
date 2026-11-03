# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pasteboard-test: NSPasteboard on the pasteboard server, installed with
# the test tools in /usr/local/bin.

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
MAN =
PROG =	pasteboard-test
SRCS =	pasteboard-test.m
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =	-framework AppKit -framework Foundation -lobjc

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/local/bin/

.include <bsd.prog.mk>
