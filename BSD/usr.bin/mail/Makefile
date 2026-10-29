# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mail, mailx: FreeBSD main's usr.bin/mail, in /usr/bin as on macOS (no
# Mail: on a case-insensitive volume it is mail); changes for Darwin are
# the lines marked Minkar. It reads the mailboxes in /var/mail (made here,
# group mail, as on macOS) and sends through /usr/sbin/sendmail when there
# is one.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	mail
SRCS =	version.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c edit.c fio.c \
	getname.c head.c v7.local.c lex.c list.c main.c names.c popen.c \
	quit.c send.c strings.c temp.c tty.c util.c vars.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/misc ${SYSROOT_DIR}/etc \
	    ${SYSROOT_DIR}/private/var/mail
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/mail
	ln -sf mail ${SYSROOT_DIR}/usr/bin/mailx
	install -m 0444 ${.CURDIR}/mail.1 ${SYSROOT_DIR}/usr/share/man/man1/
	ln -sf mail.1 ${SYSROOT_DIR}/usr/share/man/man1/mailx.1
	install -m 0444 ${.CURDIR}/misc/mail.help ${.CURDIR}/misc/mail.tildehelp \
	    ${SYSROOT_DIR}/usr/share/misc/
	install -m 0644 ${.CURDIR}/misc/mail.rc ${SYSROOT_DIR}/etc/mail.rc
	chown 0:6 ${SYSROOT_DIR}/private/var/mail
	chmod 0775 ${SYSROOT_DIR}/private/var/mail

.include <bsd.prog.mk>
