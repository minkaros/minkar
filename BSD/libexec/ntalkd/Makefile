# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ntalkd: FreeBSD main's libexec/talkd, the server talk(1) asks to find
# and invite a user, with ttymsg.c from BSD/usr.bin/wall, in /usr/libexec
# as on macOS. launchd starts it for each request on UDP port 518 (ntalk)
# through org.minkar.ntalkd, disabled as on macOS until
# "launchctl load -w". Darwin's <protocols/talkd.h> names
# the protocol's address struct osockaddr (FreeBSD's tsockaddr, the same
# layout). Changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	ntalkd
.PATH:	${ROOT_SOURCE_DIR}/BSD/usr.bin/wall
SRCS =	talkd.c announce.c process.c table.c print.c ttymsg.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-Dtsockaddr=osockaddr -include sys/ioctl.h \
	-I${ROOT_SOURCE_DIR}/BSD/usr.bin/wall \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/ntalkd
	install -m 0444 ${.CURDIR}/talkd.8 ${SYSROOT_DIR}/usr/share/man/man8/
	ln -sf talkd.8 ${SYSROOT_DIR}/usr/share/man/man8/ntalkd.8

.include <bsd.prog.mk>
