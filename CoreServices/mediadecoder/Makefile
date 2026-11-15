# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mediadecoder, AVFoundation's decoding process (AU5). Its interface is in
# Frameworks/AVFoundation/MediaDecoder, with the client. It links FFmpeg
# (BSD/lib/ffmpeg), so it is built in the BSD stage, after BSD/lib.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	mediadecoder
SRCS =	mediadecoder.c MediaDecoderServer.c
MAN =
MD = ${ROOT_SOURCE_DIR}/Frameworks/AVFoundation/MediaDecoder
MEDIA = ${MINKAR_SDKROOT}/usr/local/media
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -Wno-nullability-completeness -I${.OBJDIR} -I${MD} \
	-I${MEDIA}/include
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -L${MINKAR_SDKROOT}/usr/lib/media
LDADD += -lavformat -lavcodec -lswscale -lswresample -lavutil \
	-framework CoreFoundation -framework CoreVideo -framework IOSurface
CLEANFILES += MediaDecoderServer.c MediaDecoderServer.h

# The server side of the MIG interface.
MediaDecoderServer.c MediaDecoderServer.h: ${MD}/MediaDecoder.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${MD} \
	    -header /dev/null -user /dev/null -server MediaDecoderServer.c \
	    -sheader MediaDecoderServer.h ${.ALLSRC}

mediadecoder.o: MediaDecoderServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

.include <bsd.prog.mk>
