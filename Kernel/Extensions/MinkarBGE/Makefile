# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarBGE: driver for the Broadcom NetXtreme 57765 family (BCM57762 in
# the Mac Pro 6,1, and its siblings), on our IONetworkingFamily. The chip
# setup is FreeBSD's bge(4) for this family (BSD-4-Clause, Bill Paul and
# Wind River Systems); bge/ holds FreeBSD's register definitions
# (if_bgereg.h, trimmed to the hardware part, and brgphyreg.h).
MK_WERROR = no
SUBDIR =
KEXT = MinkarBGE
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarBGE
BUNDLE_VERSION = 1.0.0

SRCS = MinkarBGE.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR:H}/IONetworkingFamily \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
