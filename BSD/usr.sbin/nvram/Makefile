# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# nvram: our own, in /usr/sbin as on macOS, over IOKit.framework's
# NVRAM through the device tree's "options" entry.

SUBDIR =
WARNS ?= 2
PROG =	nvram
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework IOKit -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/nvram
	install -m 0444 ${.CURDIR}/nvram.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
