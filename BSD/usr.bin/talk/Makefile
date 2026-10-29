# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# talk: FreeBSD main's usr.bin/talk, in /usr/bin as on macOS, over
# ncurses; it talks to ntalkd (BSD/libexec/ntalkd) on the machines
# involved. Darwin's <protocols/talkd.h> names
# the protocol's address struct osockaddr (FreeBSD's tsockaddr, the same
# layout). Changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	talk
SRCS =	ctl.c ctl_transact.c display.c get_addrs.c get_iface.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-Dtsockaddr=osockaddr -include sys/ioctl.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lncurses

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/talk
	install -m 0444 ${.CURDIR}/talk.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
