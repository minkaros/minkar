# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AVFoundation.framework (AU5): media capture, assets and their reading,
# the sample buffer display layer, speech. Minkar drives no camera or
# microphone yet (no capture devices) and has no synthesis voices.
# Files are demuxed and decoded by FFmpeg in a process of their own,
# mediadecoder (MediaDecoder/MDClient.c).

MK_WERROR = no
FRAMEWORK = AVFoundation
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AVCapture.m AVAsset.m AVSpeech.m AVSampleBufferDisplayLayer.m AVMediaFormat.m \
	MDClient.c MediaDecoderUser.c
HEADERS = AVFoundation.h AVMediaFormat.h AVCapture.h AVAsset.h AVSpeech.h AVSampleBufferDisplayLayer.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc -I${.CURDIR}/MediaDecoder -I${.OBJDIR}
LDFLAGS += -F${FMWKS} -framework Foundation -framework CoreMedia -framework CoreVideo -framework QuartzCore \
	-framework IOSurface -lobjc
.PATH: ${.CURDIR}/MediaDecoder

# The client side of the media decoder's MIG interface (MDClient.c; the
# server is CoreServices/mediadecoder).
CLEANFILES += MediaDecoderUser.c MediaDecoderUser.h
MediaDecoderUser.c MediaDecoderUser.h: ${.CURDIR}/MediaDecoder/MediaDecoder.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${.CURDIR}/MediaDecoder \
	    -user MediaDecoderUser.c -header MediaDecoderUser.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}
MDClient.o MediaDecoderUser.o: MediaDecoderUser.h
CFLAGS.MediaDecoderUser.c = -fvisibility=hidden

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
