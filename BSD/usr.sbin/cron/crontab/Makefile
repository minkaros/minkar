# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# crontab: FreeBSD main's, set-user-ID root like on macOS, with the
# shared routines of ../lib compiled in. The spool directory
# /var/cron/tabs is created here, mode 0700.

SUBDIR =
PROG=	crontab
SRCS=	crontab.c entry.c env.c misc.c
MAN =
MK_WERROR = no
.PATH:	${.CURDIR}/../lib
CFLAGS += -I${.CURDIR}/../cron -DLOGIN_CAP \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil -I${ROOT_SOURCE_DIR}/BSD/lib/libmd
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 4555 ${PROG} ${SYSROOT_DIR}/usr/bin/crontab
	mkdir -p -m 0700 ${SYSROOT_DIR}/private/var/cron/tabs
	chmod 0700 ${SYSROOT_DIR}/private/var/cron/tabs

.include <bsd.prog.mk>
