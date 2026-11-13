# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarSandbox: the sandbox MAC policy. Keeps the profile a process gave
# itself (libsystem_sandbox's sandbox_init) with its credential and
# checks its file, network, sysctl and signal operations against it,
# with libsystem_sandbox's own evaluator. Loaded from the kernelcache as
# a security extension, before launchd.

MK_WERROR = no
SUBDIR =
KEXT = MinkarSandbox
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarSandbox
BUNDLE_VERSION = 1.0.0
MAIN_FUNCTION = MinkarSandbox_start
ANTIMAIN_FUNCTION = MinkarSandbox_stop

SANDBOX_LIB = ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_sandbox
.PATH: ${SANDBOX_LIB}
SRCS = MinkarSandbox.cpp sbeval.c sbregex.c

CFLAGS = -DPRIVATE -Wall -I${SANDBOX_LIB} \
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
