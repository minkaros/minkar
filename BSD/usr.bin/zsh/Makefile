# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# zsh 5.9, the default shell on macOS since 10.15, built with its own
# configure (cross-compiling for Darwin) and GNU make, like ncurses. The
# modules are linked into the binary: every one that builds on Darwin is
# made static (link=static in config.modules) instead of left out, so
# zmodload finds zsh/mathfunc, zsh/files, zsh/stat and the rest as on
# macOS. Installs /bin/zsh, the function library in /usr/share/zsh and
# the manual (zsh(1), zshall(1), zshbuiltins(1)...).

all: ${.OBJDIR}/.installed

# gmake gets as many jobs as this make (MAKEFLAGS is cleared: bmake's
# flags mean nothing to it).
ZSH_JOBS =	-j${.MAKE.JOBS:U1}

# Linux-only modules, and those needing libraries Minkar lacks.
ZSH_NO_MODULES = attr|cap|db/gdbm|pcre|example

${.OBJDIR}/Src/zsh: ${.CURDIR}/Makefile
	CC=${CC} \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/configure --prefix=/usr --bindir=/bin \
	    --sysconfdir=/private/etc --enable-etcdir=/private/etc \
	    --host=${CpuArch}-apple-darwin19 \
	    --disable-dynamic --disable-gdbm --disable-pcre \
	    --enable-multibyte --enable-function-subdirs \
	    --enable-maildir-support --with-tcsetpgrp
	sed -i.orig -E '\#name=zsh/(${ZSH_NO_MODULES}) #! s/ link=no / link=static /' \
	    ${.OBJDIR}/config.modules
	MAKEFLAGS="" ${TOOLS}/gmake prep
	MAKEFLAGS="" ${TOOLS}/gmake ${ZSH_JOBS}

${.OBJDIR}/.installed: ${.OBJDIR}/Src/zsh
	MAKEFLAGS="" ${TOOLS}/gmake install.bin install.fns install.man \
	    DESTDIR=${SYSROOT_DIR}
	# macOS has /bin/zsh only, not the versioned name next to it, nor the
	# zsh.old a reinstall keeps.
	rm -f ${SYSROOT_DIR}/bin/zsh-5.9 ${SYSROOT_DIR}/bin/zsh.old
	touch ${.TARGET}

