# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# dash 0.5.13.5 (BSD-3, dist/COPYING; gondor.apana.org.au/~herbert/dash),
# the small POSIX shell macOS ships as /bin/dash. dist/ holds upstream's
# release tarball (sha512 ae326c11...4c94dd, signature checked against
# Herbert Xu's key) without src/mksignames.c (GPL-2): signames.c here
# gives the same table from Darwin's <signal.h>, since that generator
# runs on the build machine and would give Linux's signal numbers. The
# other generators (mkinit, mknodes, mksyntax, mkbuiltins) only turn
# dash's own sources into tables and run on the build machine too. Built
# with its own configure, cross-compiling for Darwin, with libedit for
# set -o vi and set -o emacs.

all: ${.OBJDIR}/.installed

DASH_JOBS =	-j${.MAKE.JOBS:U1}

${.OBJDIR}/src/dash: ${.CURDIR}/Makefile ${.CURDIR}/signames.c
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist
	env -u CFLAGS -u LDFLAGS \
	CC=${CC} AR=${AR} RANLIB=${RANLIB} CC_FOR_BUILD=cc \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr --bindir=/bin \
	    --host=${CpuArch}-apple-darwin19 --with-libedit
	mkdir -p ${.OBJDIR}/src
	cp ${.CURDIR}/signames.c ${.OBJDIR}/src/signames.c
	sed -i -e '/^signames.c: mksignames/,+1d' -e '/^[a-zA-Z_ ]*=/s/ mksignames\$$(EXEEXT)//g' \
	    -e '/^[a-zA-Z_ ]*=/s/ mksignames\.c//g' -e '/^HELPERS =/s/ mksignames//' \
	    ${.OBJDIR}/src/Makefile
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake ${DASH_JOBS} -C src

${.OBJDIR}/.installed: ${.OBJDIR}/src/dash
	mkdir -p ${SYSROOT_DIR}/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/src/dash ${SYSROOT_DIR}/bin/dash
	install -m 0444 ${.CURDIR}/dist/src/dash.1 ${SYSROOT_DIR}/usr/share/man/man1/dash.1
	touch ${.TARGET}
