# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# lsof 4.99.7 (lsof-org, Purdue licence: dist/COPYING), its darwin dialect
# unchanged in dist/: it reads open files through libproc, as macOS's
# /usr/sbin/lsof does. config.h is ours (what configure finds on macOS);
# version.h and autotools.h come from dist's templates.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
MAN =
PROG =	lsof
LSOF_VERSION = 4.99.7
DIST =	${.CURDIR}/dist

.PATH:	${DIST}/lib ${DIST}/lib/dialects/darwin ${DIST}/src \
	${DIST}/src/dialects/darwin

# lib/ and src/ both have print.c and ptti.c: src's are built as src_*.
LIBSRCS = ckkv.c cvfs.c dvch.c fino.c isfn.c lkud.c lsof.c misc.c node.c \
	pdvn.c prfp.c proc.c rdev.c rnmt.c rmnt.c rnam.c rnch.c rnmh.c \
	ddev.c dfile.c dmnt.c dproc.c dsock.c dstore.c
SRCS =	${LIBSRCS} print.c ptti.c \
	arg.c main.c src_print.c src_ptti.c store.c usage.c util.c dprint.c

src_print.c: ${DIST}/src/print.c
	echo '#include "${DIST}/src/print.c"' > ${.TARGET}
src_ptti.c: ${DIST}/src/ptti.c
	echo '#include "${DIST}/src/ptti.c"' > ${.TARGET}

version.h: ${DIST}/autotools/version.h.in
	sed -e 's|@host@||; s|@logname@||; s|@user@||; s|@cc@|clang|' \
	    -e 's|@ccv@||; s|@ccflags@||; s|@ldflags@||; s|@sysinfo@|Minkar|' \
	    ${.ALLSRC} > ${.TARGET}
autotools.h: ${DIST}/autotools/autotools.h.in
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES += src_print.c src_ptti.c version.h autotools.h lsof.8

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -fvisibility=hidden \
	-DAUTOTOOLS -DDARWINV=1900 -DLSOF_VSTR=\"19.0.0\" \
	-I${.OBJDIR} -I${.CURDIR} -I${DIST}/lib/dialects/darwin -I${DIST}/lib \
	-I${DIST}/include -I${DIST}/src
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}

# The manual's two .so lines (its version, the dialect list) put inline.
lsof.8: ${DIST}/Lsof.8 ${DIST}/00DIALECTS
	sed -e 's|^\.so \./version$$|.ds VN ${LSOF_VERSION}|' \
	    -e '/^\.so \.\/00DIALECTS$$/{' -e 'r ${DIST}/00DIALECTS' -e 'd' -e '}' \
	    ${DIST}/Lsof.8 > ${.TARGET}

all: to-sysroot

to-sysroot: ${PROG} lsof.8
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/sbin/
	install -m 0444 lsof.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>

${OBJS}: version.h autotools.h
