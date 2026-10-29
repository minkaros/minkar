# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# quotacheck: our own, in /sbin as on macOS, over Darwin's quota files and
# quotactl(2) through libdqfile (BSD/lib/libdqfile).

SUBDIR =
WARNS ?= 3
PROG =	quotacheck
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libdqfile
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libdqfile -ldqfile

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/quotacheck
	install -m 0444 ${.CURDIR}/quotacheck.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
