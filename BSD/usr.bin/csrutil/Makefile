# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# csrutil: ours, System Integrity Protection's configuration from the
# kernel (csr_get_active_config), in /usr/bin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 3
PROG =	csrutil
SRCS =	csrutil.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/csrutil
	install -m 0444 ${.CURDIR}/csrutil.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
