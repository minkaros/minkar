# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ipcrm: FreeBSD main's usr.bin/ipcrm, in /usr/bin as on macOS.  It reads XNU's
# System V IPC tables through the kern.sysv.ipcs.* sysctls (ipc_minkar.h);
# other changes for Darwin are the lines marked Minkar or __APPLE__.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	ipcrm
SRCS =	ipcrm.c ipc.c
MAN =
.PATH: ${.CURDIR}/../ipcs
CFLAGS += -I${.CURDIR}/../ipcs
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/ipcrm
	install -m 0444 ${.CURDIR}/ipcrm.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
