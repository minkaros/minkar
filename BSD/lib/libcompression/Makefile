# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libcompression, ours (compression.c, compression.h), with LZFSE from the
# open lzfse (BSD-3, dist/ unchanged), zlib and liblzma:
# /usr/lib/libcompression.dylib and <compression.h>, as the reference
# system ships them.

MK_WERROR = no
LIB = compression
SUBDIR =
MAN =
.PATH: ${.CURDIR}/dist
SRCS =	compression.c lzfse_decode.c lzfse_decode_base.c lzfse_encode.c \
	lzfse_encode_base.c lzfse_fse.c lzvn_decode_base.c lzvn_encode_base.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -I${.CURDIR}/dist \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Frameworks/CoreText/xz/src/liblzma/api \
	-Wno-nullability-completeness -Wno-unknown-pragmas
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: lib${LIB}.dylib install-headers

lib${LIB}.dylib: lib${LIB}.a
	${CC} ${LDFLAGS} -o ${.TARGET} -shared -Wl,-force_load,lib${LIB}.a \
	  -Wl,-compatibility_version,1.0.0 -Wl,-current_version,1.0.0 \
	  -Wl,-install_name,/usr/lib/libcompression.dylib \
	  -L${SYSROOT_DIR}/usr/lib -lz -llzma
	mkdir -p ${SYSROOT_DIR}/usr/lib ${MINKAR_SDKROOT}/usr/lib
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/libcompression.dylib
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/libcompression.dylib

install-headers: .PHONY
	mkdir -p ${MINKAR_SDKROOT}/usr/include
	install -m 0444 ${.CURDIR}/compression.h ${MINKAR_SDKROOT}/usr/include/

.include <bsd.lib.mk>
