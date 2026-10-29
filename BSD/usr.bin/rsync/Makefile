# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# rsync 3.5.1 (GPL-3.0-or-later, dist/COPYING; rsync.samba.org). dist/
# holds upstream's release tarball
# (sha256 c55f9c9dc10fb8bec397b399a0fdded53cc9a2d8e30891bb0d63724d25c37bef,
# signature checked against Andrew Tridgell's key), unchanged. Built with
# its own configure, cross-compiling for Darwin, and GNU make, like htop:
# its own popt and zlib, Darwin's ACLs and extended attributes (-A, -X),
# iconv from libiconv; without OpenSSL, xxhash, zstd, lz4 and libidn2, which Minkar
# does not have yet (MD5 checksums and zlib compression, as rsync 3.0).
# Cross-compiling, configure cannot run its tests of socketpair, vsnprintf,
# mkstemp and readdir: Darwin's answers are given.
# Installs /usr/bin/rsync and its manuals; the man pages come prebuilt in
# the tarball.

all: ${.OBJDIR}/.installed

RSYNC_JOBS =	-j${.MAKE.JOBS:U1}

${.OBJDIR}/rsync: ${.CURDIR}/Makefile
	CC=${CC} AR=${AR} RANLIB=${RANLIB} \
	rsync_cv_HAVE_SOCKETPAIR=yes rsync_cv_HAVE_C99_VSNPRINTF=yes \
	rsync_cv_HAVE_SECURE_MKSTEMP=yes rsync_cv_HAVE_BROKEN_READDIR=no \
	rsync_cv_HAVE_BROKEN_LARGEFILE=no \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr \
	    --host=${CpuArch}-apple-darwin19 \
	    --disable-openssl --disable-xxhash --disable-zstd --disable-lz4 \
	    --disable-md2man --disable-roll-simd --disable-idn --with-included-popt \
	    --with-included-zlib
	MAKEFLAGS="" ${TOOLS}/gmake ${RSYNC_JOBS} rsync

${.OBJDIR}/.installed: ${.OBJDIR}/rsync
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/man/man5
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/rsync ${SYSROOT_DIR}/usr/bin/rsync
	install -m 0444 ${.CURDIR}/dist/rsync.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${.CURDIR}/dist/rsyncd.conf.5 ${SYSROOT_DIR}/usr/share/man/man5/
	touch ${.TARGET}
