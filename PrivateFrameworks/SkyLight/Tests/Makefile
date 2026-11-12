# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# skylight-test: the window server interface's test client (WindowServer
# plan, WS1), in /usr/local/bin like the other test tools.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	skylight-test
SRCS =	skylight-test.c
MAN =
PFW = /System/Library/PrivateFrameworks
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot \
	-F${MINKAR_SDKROOT}${PFW}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -F${MINKAR_SDKROOT}${PFW}
LDADD += -framework SkyLight

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/skylight-test

.include <bsd.prog.mk>
