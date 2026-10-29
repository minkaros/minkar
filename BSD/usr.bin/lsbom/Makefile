# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# lsbom: our own, in /usr/bin as on macOS: reads the BOM format as it is
# publicly described, checked against bills of materials Apple's tools made.

SUBDIR =
WARNS ?= 6
PROG =	lsbom
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/lsbom
	install -m 0444 ${.CURDIR}/lsbom.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
