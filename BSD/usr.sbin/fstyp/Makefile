# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fstyp: FreeBSD main's usr.sbin/fstyp, in /usr/sbin as on macOS; changes
# for Darwin are the lines marked Minkar: no geli, ufs, zfs or HAMMER (they
# need FreeBSD's libgeom, libufs and ZFS, and DragonFly's headers), Darwin's
# disk ioctls, and macOS's names for HFS+ and FAT (hfs, msdos).

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	fstyp
SRCS =	apfs.c befs.c cd9660.c exfat.c ext2fs.c fstyp.c hfsplus.c msdosfs.c \
	ntfs.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -DWITH_ICONV
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -liconv

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/fstyp
	install -m 0444 ${.CURDIR}/fstyp.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
