# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# diff3: FreeBSD main's usr.bin/diff3; on Darwin it waits for diff by pid
# (no process descriptors): the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	diff3
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
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/diff3
	install -m 0444 ${.CURDIR}/diff3.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
