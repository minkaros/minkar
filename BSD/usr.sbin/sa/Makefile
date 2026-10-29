# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# sa: FreeBSD main's usr.sbin/sa, in /usr/sbin as on macOS; changes for
# Darwin are the lines marked Minkar. FreeBSD's readrec.c (from lastcomm)
# reads FreeBSD's record formats; readrec.c and acct_darwin.h here are
# ours and read Darwin's acct(2) records (<sys/acct.h>) instead.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	sa
SRCS =	main.c db.c pdb.c usrdb.c readrec.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/sa
	install -m 0444 ${.CURDIR}/sa.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
