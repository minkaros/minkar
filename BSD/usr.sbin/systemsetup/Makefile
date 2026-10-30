# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# systemsetup: ours, the machine's settings for an administrator
# (SystemConfiguration, tzlink, launchctl), in /usr/sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	systemsetup
SRCS =	systemsetup.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework SystemConfiguration

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/systemsetup
	install -m 0444 ${.CURDIR}/systemsetup.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
