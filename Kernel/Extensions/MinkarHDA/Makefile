# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarHDA: Intel High Definition Audio controllers and their codecs (AU1).
MK_WERROR = no
SUBDIR =
KEXT = MinkarHDA
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarHDA
BUNDLE_VERSION = 1.0.0

SRCS = MinkarHDA.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
