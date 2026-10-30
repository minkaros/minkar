# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# msdos.util and the msdos.fs bundle, from Apple's msdosfs project (APSL),
# in Kernel/Extensions/msdosfs: diskarbitrationd probes FAT volumes with
# msdos.util (name, UUID) and finds fsck_msdos, mount_msdos and
# newfs_msdos through the bundle, as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
MSDOS =	${ROOT_SOURCE_DIR}/Kernel/Extensions/msdosfs
.PATH: ${MSDOS}/msdos_util.tproj
PROG =	msdos.util
SRCS =	dosutil.c
MAN =
FSB =	/System/Library/Filesystems/msdos.fs
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-deprecated-declarations
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${FSB}/Contents/Resources ${SYSROOT_DIR}/usr/share/man/man8
	sed -e 's/[$$](MSDOSFS_VERSION)/1.10/g' \
	    -e 's/[$$](PRODUCT_BUNDLE_IDENTIFIER)/com.apple.filesystems.msdos/' \
	    ${MSDOS}/Info-msdos_fs.plist > ${SYSROOT_DIR}${FSB}/Contents/Info.plist
	cp -Rf ${MSDOS}/msdos_util.tproj/en.lproj ${SYSROOT_DIR}${FSB}/Contents/Resources/
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${FSB}/Contents/Resources/
	install -m 0444 ${MSDOS}/msdos_util.tproj/msdos.util.8 ${SYSROOT_DIR}/usr/share/man/man8/
.for t in fsck_msdos mount_msdos newfs_msdos
	ln -sfn ../../../../../../sbin/${t} ${SYSROOT_DIR}${FSB}/Contents/Resources/${t}
.endfor

.include <bsd.prog.mk>
