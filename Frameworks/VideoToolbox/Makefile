# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# VideoToolbox.framework: video decoding and encoding sessions. Minkar has
# no video codec of its own here yet (hardware or software): no decoder or
# encoder is found, and apps decode by other means (FFmpeg comes in AU4).

MK_WERROR = no
FRAMEWORK = VideoToolbox
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = VTSession.c
HEADERS = VideoToolbox.h VTSession.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation -framework CoreMedia -framework CoreVideo -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
