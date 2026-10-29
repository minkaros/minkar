# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ftp: tnftp (NetBSD's ftp, BSD-2: tnftp/COPYING) as FreeBSD main builds
# it (contrib/tnftp with usr.bin/ftp's tnftp_config.h), in /usr/bin as on
# macOS, with libedit for its command line and IPv6. Changes for Darwin
# are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	ftp
.PATH:	${.CURDIR}/tnftp
SRCS =	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c \
	progressbar.c ruserpass.c util.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DINET6 -I${.CURDIR} -I${.CURDIR}/tnftp \
	-include sys/sysctl.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-ledit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/ftp
	install -m 0444 ${.CURDIR}/tnftp/ftp.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
