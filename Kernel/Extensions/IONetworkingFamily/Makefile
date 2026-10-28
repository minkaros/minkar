# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# IONetworkingFamily: our own implementation of the macOS network driver
# family (BSD-2). See README.minkar.
MK_WERROR = no
SUBDIR =
KEXT = IONetworkingFamily
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.iokit.IONetworkingFamily
BUNDLE_VERSION = 3.4.0

SRCS = IONetworkMedium.cpp IOOutputQueue.cpp IONetworkController.cpp \
	IONetworkInterface.cpp IOEthernetController.cpp IOEthernetInterface.cpp \
	IONetworkData.cpp IONetworkStack.cpp IONetworkUserClient.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

HEADERS = IOEthernetController.h IOEthernetInterface.h IONetworkController.h \
	IONetworkInterface.h IONetworkMedium.h IOOutputQueue.h IONetworkData.h \
	IONetworkStats.h IOEthernetStats.h IONetworkStack.h IONetworkUserClient.h

# The headers go in the SDK's Kernel.framework, like the other families'.
KFW_HEADERS = ${MINKAR_SDKROOT}/System/Library/Frameworks/Kernel.framework/Headers/IOKit/network

all: install-headers
install-headers:
	mkdir -p ${KFW_HEADERS}
.for h in ${HEADERS}
	cp -f ${.CURDIR}/IOKit/network/${h} ${KFW_HEADERS}/
.endfor
.PHONY: install-headers

.include <minkar.kext.mk>
