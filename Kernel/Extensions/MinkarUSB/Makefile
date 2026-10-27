# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarUSB: USB class drivers on IOUSBFamily: the composite device driver
# and HID keyboards, mice and tablets (IOHIKeyboard, IOHIPointing).
MK_WERROR = no
SUBDIR =
KEXT = MinkarUSB
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarUSB
BUNDLE_VERSION = 1.0.0

SRCS = MinkarUSB.cpp MinkarUSBKeyboard.cpp MinkarUSBPointer.cpp HIDReport.c

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-Wno-inconsistent-missing-override \
	-I${.CURDIR:H}/IOUSBFamily/include \
	-I${.CURDIR:H}/IOUSBFamily/include/IOKit/usb \
	-I${.CURDIR:H}/IOHIDFamily/IOHIDSystem \
	-I${.CURDIR:H}/IOGraphics/IOGraphicsFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <rvn.kext.mk>
