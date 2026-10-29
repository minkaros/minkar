# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# nano 9.3 (GPL-3.0-or-later, dist/COPYING; nano-editor.org), the small
# text editor macOS also ships. dist/ holds upstream's release tarball
# (sha256 a9affc52c9596ee0682bcdd4ceb3c0b235eb221d83d733dfd9b0dff45897cb0d,
# signature checked against Benno Schulenberg's key), unchanged. Built with
# its own configure, cross-compiling for Darwin, and GNU make, like htop;
# UTF-8 through the SDK's ncurses (wide, as on macOS: libncurses, not the
# host's pkg-config; configure looks for its functions in -lncursesw, so
# the answers are given), no translations (no gettext) and no libmagic.
# Installs /usr/bin/nano with rnano and pico (as on macOS) beside it, the
# syntax files in /usr/share/nano and an /etc/nanorc that uses them.

all: ${.OBJDIR}/.installed

NANO_JOBS =	-j${.MAKE.JOBS:U1}

${.OBJDIR}/src/nano: ${.CURDIR}/Makefile
	CC=${CC} AR=${AR} RANLIB=${RANLIB} PKG_CONFIG=false \
	NCURSESW_CFLAGS="-D_XOPEN_SOURCE_EXTENDED" NCURSESW_LIBS="-lncurses" \
	ac_cv_lib_ncursesw_use_default_colors=yes ac_cv_lib_ncursesw_set_escdelay=yes \
	ac_cv_lib_ncursesw_key_defined=yes ac_cv_lib_ncursesw_define_key=yes \
	gl_cv_func_wcwidth_works=yes \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr --sysconfdir=/etc \
	    --host=${CpuArch}-apple-darwin19 \
	    --enable-utf8 --disable-nls --disable-libmagic
	MAKEFLAGS="" ${TOOLS}/gmake ${NANO_JOBS}

${.OBJDIR}/.installed: ${.OBJDIR}/src/nano ${.CURDIR}/nanorc
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/man/man5 ${SYSROOT_DIR}/usr/share/nano \
	    ${SYSROOT_DIR}/etc
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/src/nano ${SYSROOT_DIR}/usr/bin/nano
	ln -sf nano ${SYSROOT_DIR}/usr/bin/rnano
	ln -sf nano ${SYSROOT_DIR}/usr/bin/pico
	install -m 0444 ${.CURDIR}/dist/doc/nano.1 ${.CURDIR}/dist/doc/rnano.1 \
	    ${SYSROOT_DIR}/usr/share/man/man1/
	ln -sf nano.1 ${SYSROOT_DIR}/usr/share/man/man1/pico.1
	install -m 0444 ${.CURDIR}/dist/doc/nanorc.5 ${SYSROOT_DIR}/usr/share/man/man5/
	install -m 0444 ${.CURDIR}/dist/syntax/*.nanorc ${SYSROOT_DIR}/usr/share/nano/
	install -m 0644 ${.CURDIR}/nanorc ${SYSROOT_DIR}/etc/nanorc
	touch ${.TARGET}
