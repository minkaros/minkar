# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# exfat.util and the exfat.fs bundle, ours: diskarbitrationd probes exFAT
# volumes with exfat.util (label, UUID) and finds fsck_exfat, mount_exfat
# and newfs_exfat through the bundle, as on macOS.

SUBDIR =
MK_WERROR = no
WARNS ?= 3
PROG =	exfat.util
SRCS =	exfat_util.c
MAN =
FSB =	/System/Library/Filesystems/exfat.fs
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${ROOT_SOURCE_DIR}/BSD/sbin/fsck_exfat -Wno-address-of-packed-member \
	-Wno-nullability-completeness -Wno-deprecated-declarations
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${FSB}/Contents/Resources ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0444 ${.CURDIR}/Info.plist ${SYSROOT_DIR}${FSB}/Contents/Info.plist
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${FSB}/Contents/Resources/
	install -m 0444 ${.CURDIR}/exfat.util.8 ${SYSROOT_DIR}/usr/share/man/man8/
.for t in fsck_exfat mount_exfat newfs_exfat
	ln -sfn ../../../../../../sbin/${t} ${SYSROOT_DIR}${FSB}/Contents/Resources/${t}
.endfor

.include <bsd.prog.mk>
