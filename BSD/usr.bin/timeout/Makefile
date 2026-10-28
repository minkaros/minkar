# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# timeout: FreeBSD main's bin/timeout. Darwin has no reaper: without
# --foreground the command gets its own process group and the signals go
# to it (the lines marked Minkar). In /usr/bin, as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	timeout
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
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/timeout
	install -m 0444 ${.CURDIR}/timeout.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
