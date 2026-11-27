# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ScreenCaptureKit.framework: capturing displays and windows. The shareable
# content comes from the window server's lists, streams image their content
# at the configured rate into IOSurface-backed pixel buffers handed out as
# sample buffers, and the content sharing picker is a panel in the calling
# application. Audio and the cursor are not captured.

MK_WERROR = no
FRAMEWORK = ScreenCaptureKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = SCShareableContent.m SCContentFilter.m SCStream.m SCContentSharingPicker.m
HEADERS = ScreenCaptureKit.h SCError.h SCShareableContent.h SCStream.h SCContentSharingPicker.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework AppKit -framework Foundation -framework CoreFoundation \
	-framework CoreGraphics -framework CoreMedia -framework CoreVideo -framework IOSurface -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
