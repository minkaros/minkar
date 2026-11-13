# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Vision.framework: image analysis requests. SHORTCUT(19.6.0): Minkar has
# no text recognition engine yet, so a request performed reports that it
# could not be done (VNErrorUnsupportedRequest) and has no results.

MK_WERROR = no
FRAMEWORK = Vision
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = Vision.m
HEADERS = Vision.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework Foundation -framework CoreGraphics -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
