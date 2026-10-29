# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# whois: FreeBSD main's usr.bin/whois; non-blocking sockets by fcntl on
# Darwin, and no canonical name for a numeric host (Darwin's getaddrinfo
# leaves it NULL): the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	whois
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/whois
	install -m 0444 ${.CURDIR}/whois.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
