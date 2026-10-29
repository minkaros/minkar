# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# liburcu 0.15.7 (userspace RCU; the library LGPL-2.1-or-later, approved
# for Minkar, with MIT and BSD-2 parts: dist/LICENSE.md; liburcu.org),
# which BIND 9.20 needs. dist/ holds upstream's userspace-rcu-0.15.7
# tarball (sha256 2556b83adc0f9b3ac8024e613e17d014d04c4c49110604ce55fcb14eae32edd3,
# signature checked against Mathieu Desnoyers's key); its tests (some
# GPL-2) are not built. Built with its own configure as shared libraries,
# so the LGPL code stays a separate, replaceable library:
# liburcu-common, liburcu (the membarrier flavour, BIND's default; it
# falls back to memory barriers where the system has no membarrier call)
# and liburcu-cds in /usr/lib; the headers stay in ${.OBJDIR}/stage.

all: ${.OBJDIR}/.installed

STAGE =		${.OBJDIR}/stage
URCU_LIBS =	liburcu-common.8 liburcu.8 liburcu-cds.8

${STAGE}/usr/lib/liburcu.8.dylib: ${.CURDIR}/Makefile
	rm -rf ${STAGE}
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist
	env -u CFLAGS -u LDFLAGS \
	CC=${CC} AR=${AR} RANLIB=${RANLIB} PKG_CONFIG=false LD=${LD} lt_cv_prog_gnu_ld=no \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr --host=${CpuArch}-apple-darwin19 \
	    --disable-static --enable-shared
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -j${.MAKE.JOBS:U1} -C src
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -C src install DESTDIR=${STAGE}
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -C include install DESTDIR=${STAGE}
	# libtool's .la files name /usr/lib paths of the build machine.
	rm -f ${STAGE}/usr/lib/*.la

${.OBJDIR}/.installed: ${STAGE}/usr/lib/liburcu.8.dylib
	mkdir -p ${SYSROOT_DIR}/usr/lib
.for l in ${URCU_LIBS}
	install -m 0555 -o 0 -g 0 ${STAGE}/usr/lib/${l}.dylib ${SYSROOT_DIR}/usr/lib/
.endfor
	touch ${.TARGET}
