# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarCodeIntegrity: the code integrity MAC policy. Marks code signatures
# CS_SIGNED, and platform binaries from the static trust cache; refuses
# restricted entitlements outside it (Docs/ADR/0001-Kernel-Entitlements.md).
# Loaded from the kernelcache as a security extension, before launchd.

MK_WERROR = no
SUBDIR =
KEXT = MinkarCodeIntegrity
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarCodeIntegrity
BUNDLE_VERSION = 1.0.0
MAIN_FUNCTION = MinkarCodeIntegrity_start
ANTIMAIN_FUNCTION = MinkarCodeIntegrity_stop

SRCS = MinkarCodeIntegrity.cpp

CFLAGS = -DPRIVATE -Wall \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/san \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/EXTERNAL_HEADERS

.include <minkar.kext.mk>
