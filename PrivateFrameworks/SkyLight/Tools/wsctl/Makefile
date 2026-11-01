# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# wsctl, the new window server's debugging tool (WindowServer plan, WS2),
# in /usr/local/bin next to the test tools.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	wsctl
SRCS =	wsctl.c
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
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/wsctl

.include <bsd.prog.mk>
