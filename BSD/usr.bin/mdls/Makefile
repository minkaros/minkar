# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mdls: our own, in /usr/bin as on the reference platform: a file's metadata
# attributes (CoreServices' MDItem, read from the file and its type).

SUBDIR =
WARNS ?= 2
PROG =	mdls
SRCS =	mdls.c mdvalue.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreServices -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/mdls
	install -C -m 0444 ${.CURDIR}/mdls.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
