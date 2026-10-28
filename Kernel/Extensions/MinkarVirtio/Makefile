# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarVirtio: virtio 1.x devices over PCI (QEMU virtio-blk-pci and
# virtio-net-pci): the block device on IOStorageFamily, the network device
# on IONetworkingFamily.
MK_WERROR = no
SUBDIR =
KEXT = MinkarVirtio
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarVirtio
BUNDLE_VERSION = 1.0.0

SRCS = MinkarVirtioPCI.cpp MinkarVirtioBlock.cpp MinkarVirtioNet.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR:H}/IOStorageFamily \
	-I${.CURDIR:H}/IONetworkingFamily \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
