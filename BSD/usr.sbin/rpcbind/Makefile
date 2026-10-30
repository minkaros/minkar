# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# rpcbind: ours, the ONC RPC binder (portmapper version 2, rpcbind 3 and 4)
# over libinfo's Sun RPC, in /usr/sbin as on macOS; launchd starts it
# (org.minkar.rpcbind) when a service first registers.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	rpcbind
SRCS =	rpcbind.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-deprecated-non-prototype
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8 \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/rpcbind
	install -m 0444 ${.CURDIR}/rpcbind.8 ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0644 ${.CURDIR}/org.minkar.rpcbind.plist \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons/

.include <bsd.prog.mk>
