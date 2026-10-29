# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# telnet: FreeBSD main's (contrib/telnet with the parts of libtelnet it
# needs), in /usr/bin as on macOS; changes for Darwin are the lines marked
# Minkar. Plain telnet: no encryption, Kerberos, OPIE or IPsec policy
# (FreeBSD adds them with libraries Minkar does not have), with
# linemode, the termios interface, IPv6 and the environment option.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	telnet
.PATH:	${.CURDIR}/telnet ${.CURDIR}/libtelnet
SRCS =	commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c \
	utilities.c genget.c getent.c misc.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DINET6 -D__APPLE_USE_RFC_3542 \
	-I${.CURDIR} -I${.CURDIR}/libtelnet \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lncurses

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/telnet
	install -m 0444 ${.CURDIR}/telnet/telnet.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
