# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CoreMedia.framework (AU5): media time (CMTime) and the keys of samples and
# their format descriptions. Sample buffers, block buffers and format
# descriptions come with AU5.

MK_WERROR = no
FRAMEWORK = CoreMedia
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CMTime.c CMConstants.c
HEADERS = CoreMedia.h CMBase.h CMTime.h CMSampleBuffer.h CMFormatDescription.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
