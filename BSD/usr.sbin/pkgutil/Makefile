# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pkgutil: ours, the receipts of installed packages in /var/db/receipts
# (their bills of materials read with lsbom), in /usr/sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	pkgutil
SRCS =	pkgutil.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/pkgutil
	install -m 0444 ${.CURDIR}/pkgutil.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
