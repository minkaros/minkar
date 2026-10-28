# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# sysctl: FreeBSD main's sbin/sysctl, in /usr/sbin as on macOS. Under
# __APPLE__: Darwin's sysctl types told apart by their format strings,
# vm.swapusage and kern.clockrate as macOS prints them, the older nodes
# without a format skipped, and no jail, loader tunable or VNET options.

SUBDIR =
MK_WERROR = no
PROG =	sysctl
MAN =
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD = -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/sbin/
	install -m 0444 ${.CURDIR}/sysctl.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
