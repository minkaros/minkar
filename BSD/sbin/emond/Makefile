# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# emond: ours, the event monitor (rules, startup and queued events),
# started by launchd for its clients, in /sbin as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	emond
SRCS =	emond.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/emond
	install -m 0444 ${.CURDIR}/emond.8 ${SYSROOT_DIR}/usr/share/man/man8/
	mkdir -p ${SYSROOT_DIR}/System/Library/LaunchDaemons \
	    ${SYSROOT_DIR}/private/etc/emond.d/rules \
	    ${SYSROOT_DIR}/private/var/db/emondClients
	install -m 0644 ${.CURDIR}/org.minkar.emond.plist \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons/

.include <bsd.prog.mk>
