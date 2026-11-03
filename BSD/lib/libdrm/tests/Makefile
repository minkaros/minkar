# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# minkar-drmtest: Minkar's libdrm as Mesa's RADV starts on it, in
# /usr/local/bin with the test tools.

SUBDIR =
MK_WERROR = no
PROG =	minkar-drmtest
SRCS =	drmtest.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot \
	-I${MINKAR_SDKROOT}/usr/include/libdrm
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -L${MINKAR_SDKROOT}/usr/lib
LDADD += -ldrm_amdgpu -ldrm

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/minkar-drmtest

.include <bsd.prog.mk>
