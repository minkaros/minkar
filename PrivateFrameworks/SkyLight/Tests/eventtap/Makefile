# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# eventtap-test: CoreGraphics' events, taps and CGEventPost through the new
# window server (WindowServer plan, WS3), in /usr/local/bin.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	eventtap-test
SRCS =	eventtap-test.c
MAN =
PFW = /System/Library/PrivateFrameworks
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot \
	-F${MINKAR_SDKROOT}${PFW}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -F${MINKAR_SDKROOT}${PFW}
LDADD += -framework SkyLight -framework CoreGraphics -framework CoreFoundation -lpthread

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/eventtap-test

.include <bsd.prog.mk>
