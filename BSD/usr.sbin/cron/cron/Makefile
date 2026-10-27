# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# cron: Vixie cron as FreeBSD main ships it, with the shared routines of
# ../lib compiled in, login classes from libbsdcompat and no PAM.
# launchd starts it (com.vix.cron, in the foreground with -n).

SUBDIR =
PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c \
	entry.c env.c misc.c
MAN =
MK_WERROR = no
.PATH:	${.CURDIR}/../lib
CFLAGS += -I${.CURDIR} -DLOGIN_CAP \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/sbin/cron

.include <bsd.prog.mk>
