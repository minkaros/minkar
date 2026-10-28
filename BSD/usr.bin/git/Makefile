# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# git 2.56.0 (GPL-2.0, dist/COPYING; approved for Minkar 2026-10-27),
# upstream's release tarball in dist/ without its tests, translations,
# git-gui, gitk and contrib (but contrib/completion),
# unchanged. Built with git's own Makefile and its Darwin settings, in a
# copy of dist/ in the object directory (git builds in its source tree).
# HTTPS through libcurl (Mbed TLS), zlib, libiconv. No Perl, Python, Tcl,
# gettext, Rust, OpenSSL or expat; no FSMonitor daemon (it needs FSEvents).
# Installs /usr/bin/git and its helpers in /usr/libexec/git-core, the
# templates in /usr/share/git-core and the release's prebuilt manual pages
# (man/, from git-manpages-2.56.0.tar.xz).

SUBDIR =
GIT_VERSION =	2.56.0
GIT_JOBS =	-j${.MAKE.JOBS:U1}
GIT_SRC =	${.OBJDIR}/src

# What git's Makefile reads from uname on a Darwin 19 host (as lsof).
GIT_UNAME =	uname_S=Darwin uname_O=Darwin uname_R=19.0.0 uname_M=x86_64 \
		uname_P=i386 uname_V=

GIT_MAKE =	cd ${GIT_SRC} && env MAKEFLAGS= ${GMAKE} ${GIT_UNAME} \
		CC="${CC}" AR="${AR}" V=1 \
		prefix=/usr gitexecdir=libexec/git-core sysconfdir=/etc \
		mandir=/usr/share/man \
		TEST_PROGRAMS= UNIT_TEST_PROGS= CLAR_TEST_PROG= \
		test_bindir_programs= FUZZ_OBJS=

all: ${.OBJDIR}/.installed

${GIT_SRC}/config.mak: ${.CURDIR}/Makefile ${.CURDIR}/dist/Makefile
	mkdir -p ${GIT_SRC}
	rsync -a --delete --exclude=/config.mak ${.CURDIR}/dist/ ${GIT_SRC}/
	{ \
	echo 'CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness'; \
	echo 'LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}'; \
	echo 'NO_PERL = YesPlease'; \
	echo 'NO_PYTHON = YesPlease'; \
	echo 'NO_TCLTK = YesPlease'; \
	echo 'NO_GETTEXT = YesPlease'; \
	echo 'NO_RUST = YesPlease'; \
	echo 'NO_OPENSSL = YesPlease'; \
	echo 'NO_APPLE_COMMON_CRYPTO = YesPlease'; \
	echo 'NO_EXPAT = YesPlease'; \
	echo 'INSTALL_SYMLINKS = YesPlease'; \
	echo 'CURL_LDFLAGS = -lcurl'; \
	echo 'FSMONITOR_DAEMON_BACKEND ='; \
	echo 'FSMONITOR_OS_SETTINGS ='; \
	echo 'BASIC_LDFLAGS := $$(subst -framework CoreServices,,$$(BASIC_LDFLAGS))'; \
	echo 'DEFAULT_PAGER = less'; \
	echo 'DEFAULT_EDITOR = vi'; \
	} > ${.TARGET}

${.OBJDIR}/.built: ${GIT_SRC}/config.mak
	${GIT_MAKE} ${GIT_JOBS} all
	touch ${.TARGET}

${.OBJDIR}/.installed: ${.OBJDIR}/.built
	${GIT_MAKE} install DESTDIR=${SYSROOT_DIR}
	# git-cvsserver is Perl: without Perl it is only a stub.
	rm -f ${SYSROOT_DIR}/usr/bin/git-cvsserver
	mkdir -p ${SYSROOT_DIR}/usr/share/man ${SYSROOT_DIR}/usr/share/git-core/contrib
	cp -R ${.CURDIR}/man/man1 ${.CURDIR}/man/man5 ${.CURDIR}/man/man7 \
	    ${SYSROOT_DIR}/usr/share/man/
	cp -R ${.CURDIR}/dist/contrib/completion \
	    ${SYSROOT_DIR}/usr/share/git-core/contrib/
	touch ${.TARGET}

clean:
	rm -rf ${GIT_SRC} ${.OBJDIR}/.built ${.OBJDIR}/.installed

.include <bsd.obj.mk>
