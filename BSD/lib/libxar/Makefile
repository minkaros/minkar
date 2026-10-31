# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxar from xar-503 (BSD-3, the original open source; dist/ is its lib/
# unchanged): /usr/lib/libxar.1.dylib and <xar/xar.h>, as the reference
# system ships them. config.h is ours, for Darwin; xar.h comes from
# xar.h.in.

MK_WERROR = no
WARNS ?= 0
LIB = xar
SUBDIR =
MAN =
.PATH: ${.CURDIR}/dist
SRCS =	archive.c arcmod.c b64.c bzxar.c darwinattr.c data.c ea.c err.c \
	ext2.c fbsdattr.c filetree.c hash.c io.c linuxattr.c lzmaxar.c \
	macho.c script.c signature.c stat.c subdoc.c util.c zxar.c

XML2 =	${MINKAR_SDKROOT}/usr/include/libxml2
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -I${.CURDIR}/dist -I${XML2} \
	-I${MINKAR_SDKROOT}/usr/include/Private \
	-I${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src \
	-include ${.CURDIR}/config.h -include stdlib.h \
	-Wno-nullability-completeness -Wno-deprecated-declarations \
	-Wno-int-conversion
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

# The library's own versions, as the reference system's: compatibility and
# current 1.0.0.
all: lib${LIB}.dylib install-headers

lib${LIB}.dylib: lib${LIB}.a
	${CC} ${LDFLAGS} -o ${.TARGET} -shared -Wl,-force_load,lib${LIB}.a \
	  -Wl,-compatibility_version,1.0.0 -Wl,-current_version,1.0.0 \
	  -Wl,-install_name,/usr/lib/libxar.1.dylib \
	  -L${SYSROOT_DIR}/usr/lib -lxml2 -lz -lbz2 -llzma -liconv
	mkdir -p ${SYSROOT_DIR}/usr/lib ${MINKAR_SDKROOT}/usr/lib
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/libxar.1.dylib
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/libxar.1.dylib
	ln -fs libxar.1.dylib ${SYSROOT_DIR}/usr/lib/libxar.dylib
	ln -fs libxar.1.dylib ${MINKAR_SDKROOT}/usr/lib/libxar.dylib

install-headers: .PHONY
	mkdir -p ${MINKAR_SDKROOT}/usr/include/xar
	install -m 0444 ${.CURDIR}/xar.h ${MINKAR_SDKROOT}/usr/include/xar/

.include <bsd.lib.mk>
