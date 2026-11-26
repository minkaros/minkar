# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AudioToolbox.framework: audio components and output units (AU3), and
# system sounds. speexdsp's resampler is built in, its symbols hidden.

MK_WERROR = no
FRAMEWORK = AudioToolbox
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AudioServices.c AUComponent.c AUPlugIn.c AUFormat.c AUOutputUnit.c AudioConverter.c \
  AudioQueue.c AudioFile.c ExtendedAudioFile.c \
  AudioFormat.c AudioFileStream.c resample.c ATCodec.c \
  AUBasicUnits.c AUGraph.c
HEADERS = AudioToolbox.h AudioServices.h AudioConverter.h \
  AudioQueue.h AudioFile.h ExtendedAudioFile.h AudioFormat.h \
  AudioFileStream.h AUGraph.h

.PATH: ${.CURDIR}/speexdsp
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS}
CFLAGS += -DOUTSIDE_SPEEX -DFLOATING_POINT -DRANDOM_PREFIX=minkar_audio \
  -DEXPORT='__attribute__((visibility("hidden")))'
LDFLAGS += -F${FMWKS} -framework CoreFoundation -framework CoreAudio
# FFmpeg's headers for the decoders (ATCodec.c); the libraries are loaded
# with dlopen when a converter first needs them, not linked.
CFLAGS += -I${MINKAR_SDKROOT}/usr/local/media/include

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.framework.mk>
