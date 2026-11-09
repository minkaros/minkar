# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Quartz.framework: Quick Look (the preview panel, and the requests generators answer).

MK_WERROR = no
FRAMEWORK = Quartz
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = QuickLook.c QLPreviewPanel.m
HEADERS = Quartz.h QuickLook.h QLPreviewPanel.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework AppKit -framework CoreGraphics -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
