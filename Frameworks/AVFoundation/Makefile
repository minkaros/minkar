# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AVFoundation.framework (AU5): media capture, assets and their reading,
# the sample buffer display layer, speech. Minkar drives no camera or
# microphone yet (no capture devices) and has no synthesis voices.
# SHORTCUT(AU5): assets have no tracks until AU4-AU5 bring the demuxers and
# decoders, and the display layer shows nothing until then.

MK_WERROR = no
FRAMEWORK = AVFoundation
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AVCapture.m AVAsset.m AVSpeech.m AVSampleBufferDisplayLayer.m AVMediaFormat.m
HEADERS = AVFoundation.h AVMediaFormat.h AVCapture.h AVAsset.h AVSpeech.h AVSampleBufferDisplayLayer.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework Foundation -framework CoreMedia -framework CoreVideo -framework QuartzCore -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
