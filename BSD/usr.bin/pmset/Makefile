# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pmset: ours, power management settings through IOKit and
# powerd, in /usr/bin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	pmset
SRCS =	pmset.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/pmset
	install -m 0444 ${.CURDIR}/pmset.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
