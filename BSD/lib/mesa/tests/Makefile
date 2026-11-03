# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# minkar-vktest: Vulkan through RADV, loaded as the Vulkan loader loads a
# driver, in /usr/local/bin with the test tools; the Vulkan headers come
# from the SDK (../ installs them).

SUBDIR =
MK_WERROR = no
PROG =	minkar-vktest
SRCS =	vktest.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/minkar-vktest

.include <bsd.prog.mk>
