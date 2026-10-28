# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# uuidgen: macOS's, written for Minkar (FreeBSD's is built on the DCE UUID
# API, not Darwin's <uuid/uuid.h>).

SUBDIR =
MK_WERROR = no
PROG =	uuidgen
SRCS =	uuidgen.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/uuidgen
	install -m 0444 ${.CURDIR}/uuidgen.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
