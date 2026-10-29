# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Vim 9.2.1252 (Vim licence, dist/LICENSE; vim.org), the editor macOS
# ships as vim. dist/ is upstream's tag v9.2.1252 from github.com/vim/vim
# (archive sha256 9e62bb4226caedf676eb70b3ba4d32b671135096c57a8ebbd63ffd423ebc5e01)
# without what Minkar does not use: its tests (src/testdir but
# Make_all.mak, which src/Makefile reads), translations
# (src/po: no gettext), the Windows installer (nsis), pixmaps and CI files.
# Built with its own configure, cross-compiling for Darwin, and GNU make:
# the huge feature set without a GUI or X11, as macOS's, on the SDK's
# ncurses. Configure's run-time tests are answered for Darwin. Installs
# /usr/bin/vim with vimdiff, rvim, rview and vimtutor, the runtime files in
# /usr/share/vim/vim92 and the manuals. vi, ex and view stay nvi's, and
# xxd is our own.

all: ${.OBJDIR}/.installed

VIM_JOBS =	-j${.MAKE.JOBS:U1}

VIM_DARWIN = vim_cv_toupper_broken=no vim_cv_terminfo=yes \
	vim_cv_tgetent=zero vim_cv_getcwd_broken=no \
	vim_cv_stat_ignores_slash=yes vim_cv_memmove_handles_overlap=yes \
	vim_cv_bcopy_handles_overlap=yes vim_cv_memcpy_handles_overlap=no \
	vim_cv_timer_create=no vim_cv_timer_create_with_lrt=no \
	vim_cv_uname_output=Darwin vim_cv_uname_r_output=19.6.0 \
	vim_cv_uname_m_output=${CpuArch}

${.OBJDIR}/src/vim: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/build && cp -R ${.CURDIR}/dist ${.OBJDIR}/build
	cd ${.OBJDIR}/build/src && \
	CC=${CC} AR=${AR} RANLIB=${RANLIB} ${VIM_DARWIN} \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	./configure --prefix=/usr --host=${CpuArch}-apple-darwin19 \
	    --with-features=huge --enable-gui=no --without-x \
	    --disable-darwin --disable-nls --disable-netbeans \
	    --disable-canberra --disable-libsodium --disable-xsmp \
	    --disable-gpm --disable-sysmouse --disable-selinux \
	    --without-wayland --with-tlib=ncurses \
	    --with-compiledby="Minkar" && \
	MAKEFLAGS="" ${TOOLS}/gmake ${VIM_JOBS}
	mkdir -p ${.OBJDIR}/src && cp ${.OBJDIR}/build/src/vim ${.TARGET}

${.OBJDIR}/.installed: ${.OBJDIR}/src/vim
	# Installed into a staging tree first, so that our xxd stays.
	rm -rf ${.OBJDIR}/stage
	cd ${.OBJDIR}/build/src && MAKEFLAGS="" ${TOOLS}/gmake installvim \
	    DESTDIR=${.OBJDIR}/stage STRIP=true
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/vim
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/stage/usr/bin/vim ${SYSROOT_DIR}/usr/bin/vim
	install -m 0555 ${.OBJDIR}/stage/usr/bin/vimtutor ${SYSROOT_DIR}/usr/bin/vimtutor
.for l in vi ex view vimdiff rvim rview
	ln -sf vim ${SYSROOT_DIR}/usr/bin/${l}
.endfor
	rm -rf ${SYSROOT_DIR}/usr/share/vim/vim92
	cp -R ${.OBJDIR}/stage/usr/share/vim/vim92 ${SYSROOT_DIR}/usr/share/vim/
.for m in vim.1 vimdiff.1 vimtutor.1
	install -m 0444 ${.OBJDIR}/stage/usr/share/man/man1/${m} ${SYSROOT_DIR}/usr/share/man/man1/
.endfor
.for l in vi ex view rvim rview
	ln -sf vim.1 ${SYSROOT_DIR}/usr/share/man/man1/${l}.1
.endfor
	touch ${.TARGET}
