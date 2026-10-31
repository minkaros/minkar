# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# notifyutil: Apple's (Libnotify, APSL), the command-line client of
# notifyd: post, register for and read notifications and their states.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
MAN =
PROG =	notifyutil
SRCS =	notifyutil.c
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-deprecated-declarations \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -m 0444 ${.CURDIR}/notifyutil.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
