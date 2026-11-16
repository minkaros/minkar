# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# FFmpeg (AU4), the decoders under Minkar's media frameworks: dynamic
# libraries under the LGPL (no --enable-gpl, no non-free parts), cut down
# to decoders, demuxers and parsers, reading files only, with dav1d for
# AV1 and the sample-format and pixel-format converters. Nothing is
# detected on its own (--disable-autodetect): no hardware decoding, and
# none of the reference platform's frameworks. Not stripped: FFmpeg strips
# its assembly's objects' local symbols, which llvm-strip does on Mach-O
# by breaking their relocations. In /usr/lib/media.

SUBDIR =
EXTLIB =	ffmpeg
EXTLIB_SYSTEM =	configure
EXTLIB_PRIVATE = media
# The system's media frameworks only with VideoToolbox or AudioToolbox.
EXTLIB_PATCHES = 0001-apple-frameworks-only-with-toolbox.patch \
		 0002-h264-decode-order-with-low-delay.patch
EXTLIB_OPTIONS = --enable-cross-compile --target-os=darwin --arch=x86_64 \
		--cc="${TC}/clang ${TARGET_FLAGS}" --cxx="${TC}/clang++ ${TARGET_FLAGS}" \
		--ld="${TC}/clang ${TARGET_FLAGS} -fuse-ld=${TC}/ld64" \
		--ar=${TC}/llvm-ar --ranlib=${TC}/llvm-ranlib --nm=${TC}/llvm-nm \
		--strip=${TC}/llvm-strip --x86asmexe=nasm --pkg-config=pkg-config \
		--host-cc=cc --extra-cflags="${WARN_FLAGS}" \
		--install-name-dir=${PRIVATE_LIB} \
		--enable-shared --disable-static --disable-debug --disable-stripping \
		--disable-autodetect --disable-programs --disable-doc \
		--disable-avdevice --disable-avfilter --disable-network \
		--disable-encoders --disable-muxers --disable-outdevs --disable-indevs \
		--disable-protocols --enable-protocol=file,pipe \
		--disable-bsfs --enable-bsf=aac_adtstoasc,h264_mp4toannexb,hevc_mp4toannexb,vp9_superframe_split,av1_frame_split \
		--enable-pthreads --enable-zlib --enable-libdav1d
EXTLIB_LIBS =	libavutil.61.dylib libswresample.7.dylib libswscale.10.dylib \
		libavcodec.63.dylib libavformat.63.dylib
EXTLIB_LICENSES = LICENSE.md COPYING.LGPLv2.1

.include <minkar.extlib.mk>
