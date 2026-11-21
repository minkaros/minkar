# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarDiskImage: disk images (a file, or a range of one) as block
# storage devices, attached and detached by hdiutil through its
# user client.
MK_WERROR = no
SUBDIR =
KEXT = MinkarDiskImage
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarDiskImage
BUNDLE_VERSION = 1.0.0

SRCS = MinkarDiskImage.cpp MinkarDiskImageCodecs.c

# Compressed UDIF images' decompressors: bzip2 (its license, BSD-style) and
# LZFSE (BSD-3), built from their sources in the tree for the kernel.
BZIP2 = ${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src
LZFSE = ${ROOT_SOURCE_DIR}/BSD/lib/libcompression/dist
.PATH: ${BZIP2} ${LZFSE}
BZIP2_SRCS = blocksort.c huffman.c crctable.c randtable.c compress.c decompress.c bzlib.c
LZFSE_SRCS = lzfse_decode.c lzfse_decode_base.c lzfse_fse.c lzvn_decode_base.c
SRCS += ${BZIP2_SRCS} ${LZFSE_SRCS}
# LZMA (ULMO): liblzma's xz decoder (0BSD), from the copy in
# Frameworks/CoreText/xz, with a configuration of its own (xz/config.h).
LZMA = ${ROOT_SOURCE_DIR}/Frameworks/CoreText/xz/src/liblzma
.PATH: ${LZMA}/common ${LZMA}/check ${LZMA}/lz ${LZMA}/lzma ${LZMA}/delta ${LZMA}/simple
LZMA_SRCS = common.c stream_buffer_decoder.c stream_decoder.c block_decoder.c \
	block_header_decoder.c filter_decoder.c filter_flags_decoder.c filter_common.c \
	index_hash.c stream_flags_decoder.c stream_flags_common.c vli_decoder.c \
	vli_size.c block_util.c check.c crc32_fast.c crc64_fast.c sha256.c \
	lz_decoder.c lzma_decoder.c lzma2_decoder.c delta_decoder.c delta_common.c \
	simple_decoder.c simple_coder.c x86.c
SRCS += ${LZMA_SRCS}
LZMA_CFLAGS = -DHAVE_CONFIG_H -DNDEBUG -I${.CURDIR}/xz -I${.CURDIR}/kshim -I${LZMA}/api \
	-I${LZMA}/common -I${LZMA}/check -I${LZMA}/lz -I${LZMA}/rangecoder -I${LZMA}/lzma \
	-I${LZMA}/delta -I${LZMA}/simple -I${LZMA}/../common \
	-Dmalloc=minkar_di_malloc -Dcalloc=minkar_di_calloc -Dfree=minkar_di_free \
	-Wno-unused-parameter
.for f in ${LZMA_SRCS}
CFLAGS.${f} += ${LZMA_CFLAGS}
.endfor
CFLAGS.MinkarDiskImageCodecs.c += -I${LZMA}/api

.for f in ${BZIP2_SRCS} MinkarDiskImageCodecs.c
CFLAGS.${f} += -I${.CURDIR}/kshim -DBZ_NO_STDIO -Dmalloc=minkar_di_malloc -Dfree=minkar_di_free \
	-Wno-unused-parameter -Wno-implicit-fallthrough
.endfor
.for f in ${LZFSE_SRCS}
CFLAGS.${f} += -I${.CURDIR}/kshim -DNDEBUG -Dmalloc=minkar_di_malloc -Dfree=minkar_di_free \
	-Wno-unused-parameter
.endfor

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${BZIP2} -I${LZFSE} \
	-I${.CURDIR:H}/IOStorageFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
