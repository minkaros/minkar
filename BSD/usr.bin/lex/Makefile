# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# lex, flex, flex++: flex as FreeBSD main builds it (contrib/flex/src,
# flex's BSD-style licence in flex/COPYING, and usr.bin/lex's config.h),
# from FreeBSD's pre-made scanner, parser and skeleton (initscan.c,
# initparse.c with its parse.h, initskel.c, made from flex/scan.l,
# parse.y and flex.skl), so the build needs no lex, yacc or m4. The
# scanners it writes go through m4 when it runs.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	lex
.PATH:	${.CURDIR}/flex
SRCS =	buf.c ccl.c dfa.c ecs.c filter.c gen.c main.c misc.c nfa.c \
	options.c initparse.c initscan.c regex.c scanflags.c scanopt.c initskel.c \
	sym.c tables.c tables_shared.c tblcmp.c yylex.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} -I${.CURDIR}/flex \
	-DHAVE_CONFIG_H \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/include
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/lex
.for l in flex flex++
	ln -sf lex ${SYSROOT_DIR}/usr/bin/${l}
	ln -sf lex.1 ${SYSROOT_DIR}/usr/share/man/man1/${l}.1
.endfor
	install -m 0444 ${.CURDIR}/lex.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${.CURDIR}/flex/FlexLexer.h ${SYSROOT_DIR}/usr/include/

.include <bsd.prog.mk>
