# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# bc and dc: Gavin Howard's bc (BSD-2) as FreeBSD main ships it
# (contrib/bc), with FreeBSD's defaults, its own line history instead of
# libedit and no message catalogs. dc is a link: the program picks its
# language from its name.

SUBDIR =
PROG=	bc
MAN =
MK_WERROR = no
VERSION!=	cat ${.CURDIR}/VERSION.txt
.PATH:	${.CURDIR}/src ${.OBJDIR}
SRCS=	args.c bc.c bc_lex.c bc_parse.c data.c dc.c dc_lex.c dc_parse.c \
	file.c history.c lang.c lex.c main.c num.c opt.c parse.c program.c \
	rand.c read.c vector.c vm.c bc_help.c dc_help.c lib.c lib2.c
CLEANFILES+=	bc_help.c dc_help.c lib.c lib2.c

CFLAGS += -I${.CURDIR}/include \
	-DBC_DEFAULT_BANNER=0 -DBC_DEFAULT_DIGIT_CLAMP=0 \
	-DBC_DEFAULT_EXPR_EXIT=1 -DBC_DEFAULT_PROMPT=0 \
	-DBC_DEFAULT_SIGINT_RESET=1 -DBC_DEFAULT_TTY_MODE=1 \
	-DBC_ENABLED=1 -DBC_ENABLE_AFL=0 -DBC_ENABLE_EDITLINE=0 \
	-DBC_ENABLE_EXTRA_MATH=1 -DBC_ENABLE_HISTORY=1 -DBC_ENABLE_LIBRARY=0 \
	-DBC_ENABLE_MEMCHECK=0 -DBC_EXCLUDE_EXTRA_MATH=0 -DBC_ENABLE_NLS=0 \
	-DDC_DEFAULT_DIGIT_CLAMP=0 -DDC_DEFAULT_EXPR_EXIT=1 \
	-DDC_DEFAULT_PROMPT=0 -DDC_DEFAULT_SIGINT_RESET=1 \
	-DDC_DEFAULT_TTY_MODE=0 -DDC_ENABLED=1 \
	-DBUILD_TYPE=A -DMAINEXEC=bc -DNDEBUG -DVERSION=${VERSION}

lib.c: ${.CURDIR}/gen/lib.bc
	cd ${.CURDIR} && sh gen/strgen.sh gen/lib.bc ${.OBJDIR}/lib.c 0 bc_lib bc_lib_name "" 1
lib2.c: ${.CURDIR}/gen/lib2.bc
	cd ${.CURDIR} && sh gen/strgen.sh gen/lib2.bc ${.OBJDIR}/lib2.c 0 bc_lib2 bc_lib2_name "" 1
bc_help.c: ${.CURDIR}/gen/bc_help.txt
	cd ${.CURDIR} && sh gen/strgen.sh gen/bc_help.txt ${.OBJDIR}/bc_help.c 0 bc_help "" 1 0
dc_help.c: ${.CURDIR}/gen/dc_help.txt
	cd ${.CURDIR} && sh gen/strgen.sh gen/dc_help.txt ${.OBJDIR}/dc_help.c 0 dc_help "" 1 0

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/bc
	ln -f ${SYSROOT_DIR}/usr/bin/bc ${SYSROOT_DIR}/usr/bin/dc

.include <bsd.prog.mk>
