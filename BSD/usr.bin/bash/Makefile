# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# bash 5.3 with its official patches 001-020 (GPL-3.0-or-later,
# dist/COPYING; gnu.org/software/bash). dist/ holds upstream's release
# tarball (sha256 0d5cd86965f869a26cf64f4b71be7b96f90a3ba8b3d74e27e8e9d9d5550f31ba)
# with the patches applied, signatures checked against Chet Ramey's key in
# the GNU keyring; otherwise unchanged. macOS ships bash 3.2, the last
# under GPL-2; scripts written for it run on 5.3. Built with its own
# configure (cross-compiling for Darwin; the helper programs it runs while
# building, with the host's compiler) and GNU make, like zsh; its own
# readline, the SDK's ncurses, no translations. Installs /bin/bash and
# bash(1).

all: ${.OBJDIR}/.installed

# What configure cannot run when cross-compiling, answered for Darwin
# (as bash's configure finds on macOS): the exit status sits 8 bits up in
# wait's status, named pipes and sys_siglist exist, printf has %a, the
# snprintf family is standard, getcwd(NULL, 0) allocates, WCONTINUED works.
BASH_DARWIN = bash_cv_wexitstatus_offset=8 bash_cv_sys_named_pipes=present \
	bash_cv_sys_siglist=yes bash_cv_under_sys_siglist=no \
	bash_cv_printf_a_format=yes bash_cv_func_snprintf=yes \
	bash_cv_func_vsnprintf=yes bash_cv_getcwd_malloc=yes \
	bash_cv_wcontinued_broken=no bash_cv_func_sigsetjmp=present \
	bash_cv_must_reinstall_sighandlers=no bash_cv_opendir_not_robust=no \
	bash_cv_dup2_broken=no bash_cv_pgrp_pipe=no bash_cv_unusable_rtsigs=no \
	bash_cv_ulimit_maxfds=yes bash_cv_getenv_redef=yes

BASH_JOBS =	-j${.MAKE.JOBS:U1}

${.OBJDIR}/bash: ${.CURDIR}/Makefile
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist
	CC=${CC} AR=${AR} RANLIB=${RANLIB} CC_FOR_BUILD=cc \
	${BASH_DARWIN:@v@${v} @} \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr --bindir=/bin \
	    --host=${CpuArch}-apple-darwin19 \
	    --without-bash-malloc --disable-nls --with-curses
	MAKEFLAGS="" ${TOOLS}/gmake ${BASH_JOBS}

${.OBJDIR}/.installed: ${.OBJDIR}/bash
	mkdir -p ${SYSROOT_DIR}/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/bash ${SYSROOT_DIR}/bin/bash
	install -m 0444 ${.CURDIR}/dist/doc/bash.1 ${SYSROOT_DIR}/usr/share/man/man1/
	touch ${.TARGET}
