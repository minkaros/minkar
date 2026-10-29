# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Zip 3.0 (Info-ZIP licence, dist/LICENSE; info-zip.org), macOS's zip.
# dist/ holds upstream's zip30.tar.gz (sha256
# f0e8bb1f9b7eb0b01285495a2699df3a4b766784c1765a8f1aeedf63c0806369, the
# same as Debian's orig tarball) without the folders of other systems.
# Upstream has not released since 2008: patches/debian holds Debian's
# zip 3.0-16 fixes (buffer overflows, the command injection in -TT,
# symlink updates...), applied in their series order to a copy in the
# object folder, and patches/minkar-* says, as the licence asks of
# altered versions, that this one is altered for Minkar. Built with
# upstream's unix/Makefile and the flags its unix/configure finds on
# Darwin (it runs test programs, so it cannot cross-compile): large
# files and Zip64, Unicode names, bzip2 through the system's libbz2.

all: ${.OBJDIR}/.installed

BZ =		${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src
ZIP_LOC =	-DUNIX -DBZIP2_SUPPORT -DUIDGID_NOT_16BIT -DLARGE_FILE_SUPPORT \
		-DUNICODE_SUPPORT -DHAVE_DIRENT_H -DHAVE_TERMIOS_H -I${BZ}

${.OBJDIR}/build/zip: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/build
	cp -R ${.CURDIR}/dist ${.OBJDIR}/build
.for p in ${:!grep -v '^\#' ${.CURDIR}/patches/debian/series!}
	cd ${.OBJDIR}/build && patch -s -p1 < ${.CURDIR}/patches/debian/${p}
.endfor
	cd ${.OBJDIR}/build && patch -s -p1 < ${.CURDIR}/patches/minkar-altered-version.patch
	cd ${.OBJDIR}/build && env -u CFLAGS -u LDFLAGS MAKEFLAGS="" \
	    ${TOOLS}/gmake -j${.MAKE.JOBS:U1} -f unix/Makefile zip \
	    CC=${CC} BIND=${CC} IZ_BZIP2=${BZ} LIB_BZ= \
	    CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness -I. ${ZIP_LOC}" \
	    LFLAGS1="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	    LFLAGS2="-L${SYSROOT_DIR}/usr/lib -lbz2"

${.OBJDIR}/.installed: ${.OBJDIR}/build/zip
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/build/zip ${SYSROOT_DIR}/usr/bin/zip
	install -m 0444 ${.OBJDIR}/build/man/zip.1 ${SYSROOT_DIR}/usr/share/man/man1/zip.1
	touch ${.TARGET}
