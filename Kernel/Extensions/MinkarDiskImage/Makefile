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
