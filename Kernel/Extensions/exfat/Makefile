# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# exfat: ours, after Microsoft's published exFAT specification. Reads
# exFAT volumes (read only for now); its on-disk format is the header
# fsck_exfat and newfs_exfat use.

MK_WERROR = no
SUBDIR =
KEXT = exfat
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.filesystems.exfat
BUNDLE_VERSION = 1.0.0
MAIN_FUNCTION = exfat_module_start
ANTIMAIN_FUNCTION = exfat_module_stop

SRCS =	exfat_subr.c \
	exfat_vfsops.c \
	exfat_vnops.c \
	exfat_loader.cpp

CFLAGS = -DKERNEL=1 -Wall -Wno-address-of-packed-member \
	-I${.CURDIR} -I${ROOT_SOURCE_DIR}/BSD/sbin/fsck_exfat \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/san \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/EXTERNAL_HEADERS

.include <minkar.kext.mk>
