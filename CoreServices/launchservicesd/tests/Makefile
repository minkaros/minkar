# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# lssession-test: launchservicesd's end-to-end test (see lssession-test.c),
# in /usr/local/bin. Built in the CoreServices stage, after CoreServices.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	lssession-test
SRCS =	lssession-test.c
MAN =
LS_FRAMEWORKS = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Frameworks
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot -F${LS_FRAMEWORKS}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreServices

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/${PROG}

.include <bsd.prog.mk>
