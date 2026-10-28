# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarE1000: driver for Intel 8254x and 82574L Ethernet (QEMU e1000 and
# e1000e, VirtualBox, VMware), on our IONetworkingFamily. e1000/ is Intel's
# shared code for the whole family (FreeBSD's sys/dev/e1000, BSD-3-Clause,
# unchanged but for e1000_osdep.h, which is Minkar's), for the chips that
# need it: the PCH's I217-I219 and the I210/I211.
MK_WERROR = no
SUBDIR =
KEXT = MinkarE1000
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarE1000
BUNDLE_VERSION = 1.0.0

SRCS = MinkarE1000.cpp E1000Shared.cpp
E1000_SHARED = e1000_80003es2lan.c e1000_82540.c e1000_82541.c e1000_82542.c \
	e1000_82543.c e1000_82571.c e1000_82575.c e1000_api.c e1000_base.c \
	e1000_i210.c e1000_ich8lan.c e1000_mac.c e1000_manage.c e1000_mbx.c \
	e1000_nvm.c e1000_phy.c e1000_vf.c
SRCS += ${E1000_SHARED}
.PATH: ${.CURDIR}/e1000

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR}/e1000 \
	-I${.CURDIR:H}/IONetworkingFamily \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
