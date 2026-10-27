# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ps: FreeBSD main's bin/ps; the macOS options, displays and keywords are
# under __APPLE__. Processes come from libbsdcompat's kvm_darwin.c.

SUBDIR =
MK_WERROR = no
PROG =	ps
SRCS =	fmt.c keyword.c nlist.c print.c ps.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lxo

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/
	install -m 0444 ${.CURDIR}/ps.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
