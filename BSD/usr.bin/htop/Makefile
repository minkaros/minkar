# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# htop 3.4.1 (GPL-2.0, dist/COPYING; htop.dev), the interactive process
# viewer, on its Darwin backend (dist/darwin: libproc, the Mach host
# interface, sysctl and IOKit). dist/ holds upstream's release tarball
# (sha256 904f7d4580fc11cffc7e0f06895a4789e0c1c054435752c151e812fead9f6220)
# without autom4te.cache, unchanged. Built with its own configure, cross-
# compiling for Darwin, and GNU make, like zsh; Unicode through ncurses.
# Installs /usr/bin/htop and htop(1) only: the desktop entry and icons
# have no use on Minkar.

all: ${.OBJDIR}/.installed

HTOP_JOBS =	-j${.MAKE.JOBS:U1}

${.OBJDIR}/htop: ${.CURDIR}/Makefile
	CC=${CC} \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr \
	    --host=${CpuArch}-apple-darwin19 \
	    --enable-unicode --disable-hwloc --disable-affinity
	MAKEFLAGS="" ${TOOLS}/gmake ${HTOP_JOBS} htop htop.1

${.OBJDIR}/.installed: ${.OBJDIR}/htop
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/htop ${SYSROOT_DIR}/usr/bin/htop
	install -m 0444 ${.OBJDIR}/htop.1 ${SYSROOT_DIR}/usr/share/man/man1/
	touch ${.TARGET}

