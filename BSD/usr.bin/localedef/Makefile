# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# localedef: FreeBSD main's usr.bin/localedef (Nexenta's, BSD-2), in
# /usr/bin as on macOS, built the way FreeBSD bootstraps it on other
# systems (bootstrap/, with the FreeBSD libc headers it needs in libc/).
# It writes what Darwin's libc reads: LC_CTYPE as "RuneMagA" and
# LC_MESSAGES/LC_MESSAGES (darwin_out.c, ours); LC_TIME, LC_NUMERIC,
# LC_MONETARY and LC_MESSAGES are the same text as FreeBSD's; LC_COLLATE
# is code point order, as Minkar's libc collates. Its manual is ours (the
# upstream one is CDDL). Changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	localedef
SRCS =	charmap.c collate.c ctype.c localedef.c messages.c monetary.c \
	numeric.c parser.y scanner.c time.c wide.c darwin_out.c
MAN =
YFLAGS =	-d
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I. -I${.CURDIR} \
	-I${.CURDIR}/bootstrap -I${.CURDIR}/libc \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

${SRCS:M*.c}: parser.h
parser.h: parser.y

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/localedef
	install -m 0444 ${.CURDIR}/localedef.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
