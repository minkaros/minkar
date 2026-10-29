# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# tcsh 6.24.16 (BSD-3, dist/Copyright; astron.com/pub/tcsh), the C shell
# macOS ships as /bin/tcsh and /bin/csh. dist/ holds upstream's release
# tarball (sha256 4208cf4630fb64d91d81987f854f9570a5a0e8a001a92827def37d0ed8f37364,
# signature checked against Kimmo Suominen's key) without the win32,
# cygwin, debian and svn folders. Built with its own configure,
# cross-compiling for Darwin; gethost, which writes tc.defs.c while
# building, runs on the build machine. Multibyte (UTF-8) text; no
# message catalogs (as on macOS, the messages are the built-in English
# ones). csh.cshrc, csh.login and csh.logout are the system-wide start
# files, empty but for comments, in /etc.

all: ${.OBJDIR}/.installed

TCSH_JOBS =	-j${.MAKE.JOBS:U1}

# What configure cannot run when cross-compiling, answered for Darwin.
TCSH_DARWIN =	ac_cv_func_getpgrp_void=yes ac_cv_func_mbrtowc=yes \
	ac_cv_func_strcoll_works=yes

${.OBJDIR}/tcsh: ${.CURDIR}/Makefile
	env -u CFLAGS -u LDFLAGS \
	CC=${CC} AR=${AR} RANLIB=${RANLIB} ${TCSH_DARWIN} \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	CC_FOR_GETHOST=cc \
	${.CURDIR}/dist/configure --prefix=/usr --bindir=/bin \
	    --host=${CpuArch}-apple-darwin19 --disable-nls-catalogs
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake ${TCSH_JOBS} \
	    CPPFLAGS_FOR_BUILD="-I. -I${.CURDIR}/dist" tcsh tcsh.man

${.OBJDIR}/.installed: ${.OBJDIR}/tcsh ${.CURDIR}/csh.cshrc ${.CURDIR}/csh.login \
    ${.CURDIR}/csh.logout
	mkdir -p ${SYSROOT_DIR}/bin ${SYSROOT_DIR}/etc ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/tcsh ${SYSROOT_DIR}/bin/tcsh
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/tcsh ${SYSROOT_DIR}/bin/csh
	install -m 0444 ${.OBJDIR}/tcsh.man ${SYSROOT_DIR}/usr/share/man/man1/tcsh.1
	ln -sf tcsh.1 ${SYSROOT_DIR}/usr/share/man/man1/csh.1
.for f in csh.cshrc csh.login csh.logout
	install -m 0644 ${.CURDIR}/${f} ${SYSROOT_DIR}/etc/${f}
.endfor
	touch ${.TARGET}
