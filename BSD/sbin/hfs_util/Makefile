# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# hfs.util and the hfs.fs bundle, from Apple's hfs project (APSL), in
# Kernel/Extensions/hfs: diskarbitrationd probes HFS Plus volumes with
# hfs.util (name, UUID) and finds fsck_hfs, mount_hfs and newfs_hfs
# through the bundle, as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
HFS = ${ROOT_SOURCE_DIR}/Kernel/Extensions/hfs
.PATH: ${HFS}/hfs_util
PROG =	hfs.util
SRCS =	hfsutil_main.c hfsutil_jnl.c
MAN =
FSB =	/System/Library/Filesystems/hfs.fs
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${ROOT_SOURCE_DIR}/BSD/sbin/mount_hfs/include -I${HFS}/core \
	-Wno-deprecated-declarations
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${FSB}/Contents/Resources ${SYSROOT_DIR}/usr/share/man/man8
	sed -e 's/[$$]HFS_VERSION/556.100.11/' \
	    -e 's/[$$]HFS_COPYRIGHT_INFO_STRING/556.100.11, Copyright Apple Inc. 1999-2019/' \
	    ${HFS}/fs/Info.plist > ${SYSROOT_DIR}${FSB}/Contents/Info.plist
	cp -Rf ${HFS}/fs/en.lproj ${SYSROOT_DIR}${FSB}/Contents/Resources/
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${FSB}/Contents/Resources/
	install -m 0444 ${HFS}/hfs_util/hfs.util.8 ${SYSROOT_DIR}/usr/share/man/man8/
.for t in fsck_hfs mount_hfs newfs_hfs
	ln -sfn ../../../../../../sbin/${t} ${SYSROOT_DIR}${FSB}/Contents/Resources/${t}
.endfor

.include <bsd.prog.mk>
