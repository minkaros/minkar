# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ImageIO.framework: image sources and destinations. Onyx2D does the
# work and CoreGraphics exports it; this framework re-exports
# CoreGraphics, as binaries for the reference platform import the
# functions from ImageIO.

MK_WERROR = no
FRAMEWORK = ImageIO
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = ImageIO.c
HEADERS = ImageIO.h CGImageSource.h CGImageDestination.h CGImageProperties.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
LDFLAGS += -F${FMWKS} -Wl,-reexport_framework,CoreGraphics

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
