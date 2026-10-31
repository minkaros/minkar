# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# system_profiler: ours, the machine's hardware and software by data type
# (sysctl, IOKit's registry, getifaddrs), in /usr/sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	system_profiler
SRCS =	system_profiler.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/system_profiler
	install -m 0444 ${.CURDIR}/system_profiler.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
