# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# dhcpclient: the DHCP client daemon, started by launchd (architecture plan, Phase 2).

SUBDIR =

PROG =	dhcpclient
SRCS =	main.c iface.c dhcp4.c ipv6.c dhcp6.c store.c
MAN =

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness \
	-I${MINKAR_SDKROOT}/System/Library/Frameworks/System.framework/PrivateHeaders
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
# configd (store.c); the CoreFoundation headers warn under -Wsystem-headers.
CFLAGS.store.c = -Wno-system-headers
LDADD =	-framework SystemConfiguration -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	cp -f ${PROG} ${SYSROOT_DIR}/usr/libexec/

.include <bsd.prog.mk>
