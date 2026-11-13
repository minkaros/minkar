# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MediaPlayer.framework: what an app is playing (MPNowPlayingInfoCenter) and
# the commands it takes from outside (MPRemoteCommandCenter: play, pause,
# next, seek). The app's now-playing information is also posted to the
# session (org.minkar.MediaPlayer.NowPlayingDidChange) for the desktop to show.

MK_WERROR = no
FRAMEWORK = MediaPlayer
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = MPNowPlayingInfoCenter.m MPRemoteCommandCenter.m
HEADERS = MediaPlayer.h MPNowPlayingInfoCenter.h MPRemoteCommandCenter.h MPMediaItem.h
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
