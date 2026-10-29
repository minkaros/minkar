# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# wall: FreeBSD main's usr.bin/wall, unchanged. Group tty is
# gid 4 on Minkar (as on macOS), not the build host's tty.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	wall
SRCS =	ttymsg.c wall.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 2555 -g 4 ${PROG} ${SYSROOT_DIR}/usr/bin/wall
	install -m 0444 ${.CURDIR}/wall.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
