# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mount_nfs: Apple's, from NFS-136.100.3 (macOS 10.15.6; APSL and BSD, each
# file's header; github.com/apple-oss-distributions/NFS, tarball sha256
# dba4617e53c1a2717691fea1d605851ff62929fd3cf5f6d3bbbed315771c450a), for
# the NFS client xnu 19.6 carries. dist/ holds the files it uses, with
# /etc/nfs.conf and the nfs(5) and nfs.conf(5) manuals. Changes for Darwin
# are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
PROG =	mount_nfs
SRCS =	mount_nfs.c
MAN =
.PATH: ${.CURDIR}/dist
LIBUTIL =	${ROOT_SOURCE_DIR}/BSD/lib/libutil
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DPRIVATE -I${LIBUTIL}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${LIBUTIL} -lutil -framework CoreFoundation -framework IOKit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/etc ${SYSROOT_DIR}/usr/share/man/man5 \
	    ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/mount_nfs
	install -m 0644 ${.CURDIR}/dist/nfs.conf ${SYSROOT_DIR}/etc/nfs.conf
	install -m 0444 ${.CURDIR}/dist/mount_nfs.8 ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0444 ${.CURDIR}/dist/nfs.5 ${.CURDIR}/dist/nfs.conf.5 \
	    ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>
