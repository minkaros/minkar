# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mdfind: our own, in /usr/bin as on the reference platform: files that
# match a metadata query, searched on the disk (there is no index yet).

SUBDIR =
WARNS ?= 2
PROG =	mdfind
SRCS =	mdfind.c mdvalue.c
.PATH: ${.CURDIR}/../mdls
CFLAGS += -I${.CURDIR}/../mdls
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreServices -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/mdfind
	install -C -m 0444 ${.CURDIR}/mdfind.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
