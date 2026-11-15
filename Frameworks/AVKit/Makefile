# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AVKit.framework (AU5): AVPlayerView, a view that shows a player's video
# with its playback controls.

SUBDIR =
MK_WERROR = no
FRAMEWORK = AVKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AVPlayerView.m
HEADERS = AVKit.h AVPlayerView.h

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework AppKit -framework Foundation -framework AVFoundation -framework CoreMedia \
	-framework QuartzCore -framework CoreGraphics -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
