# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ncal and cal: FreeBSD main's usr.bin/ncal, with lib/libcalendar's
# calendar.c and easter.c built in (FreeBSD keeps them in a library
# nothing else here uses). Today is highlighted through ncurses' termcap.

SUBDIR =
MK_WERROR = no
PROG =	ncal
SRCS =	ncal.c calendar.c easter.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lncurses

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/ncal
	ln -f ${SYSROOT_DIR}/usr/bin/ncal ${SYSROOT_DIR}/usr/bin/cal
	install -m 0444 ${.CURDIR}/ncal.1 ${SYSROOT_DIR}/usr/share/man/man1/
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/ncal.1 ${SYSROOT_DIR}/usr/share/man/man1/cal.1

.include <bsd.prog.mk>
