# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# gzip, gunzip, zcat and gzcat: FreeBSD's (NetBSD's) gzip (BSD-2), with
# its scripts gzexe, zdiff, zforce, zmore and znew (ISC). It reads gzip,
# bzip2, xz, lzip, pack and compress files; zstd is left out (no libzstd).
# Makefile.freebsd is the original.

SUBDIR =
MK_WERROR = no
PROG =	gzip
MAN =
BINDIR = /usr/bin
SCRIPTS = gzexe zdiff zforce zmore znew

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DNO_ZSTD_SUPPORT \
	-I${ROOT_SOURCE_DIR}/Libraries/zlib -I${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src \
	-I${ROOT_SOURCE_DIR}/Frameworks/CoreText/xz/src/liblzma/api \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lz -L${SYSROOT_DIR}/usr/lib -lbz2 -llzma

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR} ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/
.for l in gunzip gzcat zcat
	ln -f ${SYSROOT_DIR}${BINDIR}/gzip ${SYSROOT_DIR}${BINDIR}/${l}
.endfor
.for s in ${SCRIPTS}
	install -m 0555 ${.CURDIR}/${s} ${SYSROOT_DIR}${BINDIR}/
.endfor
	ln -f ${SYSROOT_DIR}${BINDIR}/zdiff ${SYSROOT_DIR}${BINDIR}/zcmp
	ln -f ${SYSROOT_DIR}${BINDIR}/zmore ${SYSROOT_DIR}${BINDIR}/zless
.for m in gzip.1 gzexe.1 zdiff.1 zforce.1 zmore.1 znew.1
	install -m 0444 ${.CURDIR}/${m} ${SYSROOT_DIR}/usr/share/man/man1/
.endfor

.include <bsd.prog.mk>
