# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# UnZip 6.0 (Info-ZIP licence, dist/LICENSE; info-zip.org), macOS's
# unzip, zipinfo and zipgrep. dist/ holds upstream's unzip60.tar.gz
# (sha256 036d96991646d0449ed0aa952e4fbe21b476ce994abc276e49d30e686708bd37,
# the same as Debian's orig tarball) without the folders of other
# systems. Upstream has not released since 2009: patches/debian holds
# Debian's unzip 6.0-31 fixes (the CVEs, zip bombs, Unicode names,
# timestamps...) without its branding patch, applied in their series
# order to a copy in the object folder, and patches/minkar-* says, as the
# licence asks of altered versions, that this one is altered for Minkar.
# Built with upstream's unix/Makefile and the flags its unix/configure
# finds on Darwin (it runs test programs, so it cannot cross-compile):
# large files and Zip64, Unicode names in UTF-8, bzip2 through the
# system's libbz2. zipinfo is unzip under another name.

all: ${.OBJDIR}/.installed

BZ =		${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src
UNZIP_LOC =	-DLARGE_FILE_SUPPORT -DUNICODE_SUPPORT -DUNICODE_WCHAR \
		-DUTF8_MAYBE_NATIVE -DHAVE_DIRENT_H -DHAVE_TERMIOS_H -D_MBCS \
		-DUSE_BZIP2

${.OBJDIR}/build/unzip: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/build
	cp -R ${.CURDIR}/dist ${.OBJDIR}/build
.for p in ${:!grep -v '^\#' ${.CURDIR}/patches/debian/series!}
	cd ${.OBJDIR}/build && patch -s -p1 < ${.CURDIR}/patches/debian/${p}
.endfor
	cd ${.OBJDIR}/build && patch -s -p1 < ${.CURDIR}/patches/minkar-altered-version.patch
	cd ${.OBJDIR}/build && env -u CFLAGS -u LDFLAGS MAKEFLAGS="" \
	    ${TOOLS}/gmake -j${.MAKE.JOBS:U1} -f unix/Makefile unzip \
	    CC=${CC} LD=${CC} IZ_BZIP2=${BZ} D_USE_BZ2= LIBBZ2= \
	    CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -O2 -Wno-nullability-completeness" \
	    LOCAL_UNZIP="${UNZIP_LOC}" \
	    L_BZ2="-L${SYSROOT_DIR}/usr/lib -lbz2" \
	    LF2="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}"

${.OBJDIR}/.installed: ${.OBJDIR}/build/unzip
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/build/unzip ${SYSROOT_DIR}/usr/bin/unzip
	ln -sf unzip ${SYSROOT_DIR}/usr/bin/zipinfo
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/build/unix/zipgrep ${SYSROOT_DIR}/usr/bin/zipgrep
.for m in unzip zipinfo zipgrep
	install -m 0444 ${.OBJDIR}/build/man/${m}.1 ${SYSROOT_DIR}/usr/share/man/man1/${m}.1
.endfor
	touch ${.TARGET}
