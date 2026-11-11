# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# QuickLook.framework: the Quick Look generators' API (thumbnail and
# preview requests) and QLThumbnailImageCreate.

SUBDIR =
MK_WERROR = no
FRAMEWORK = QuickLook
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = QuickLook.c
HEADERS = QuickLook.h QLBase.h QLGenerator.h

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -Wno-nullability-completeness
LDFLAGS += -F${FMWKS} -framework CoreGraphics -framework CoreFoundation

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
