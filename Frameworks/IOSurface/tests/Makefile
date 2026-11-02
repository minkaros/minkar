# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# iosurface-test: IOSurface.framework end to end, in /usr/local/bin.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	iosurface-test
SRCS =	iosurface-test.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot -Wno-four-char-constants
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework IOSurface -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/${PROG}

.include <bsd.prog.mk>
