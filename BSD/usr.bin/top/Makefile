# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# top: our own (BSD-2), with macOS's options, on libproc, the Mach host
# interface, sysctl and getifaddrs.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	top
SRCS =	top.c
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -m 0444 ${.CURDIR}/top.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
