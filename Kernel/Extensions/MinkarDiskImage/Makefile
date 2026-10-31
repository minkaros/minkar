# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarDiskImage: disk images (a file, or a range of one) as block
# storage devices, attached and detached by hdiutil through its
# user client.
MK_WERROR = no
SUBDIR =
KEXT = MinkarDiskImage
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarDiskImage
BUNDLE_VERSION = 1.0.0

SRCS = MinkarDiskImage.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR:H}/IOStorageFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
