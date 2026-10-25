# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# file(1) 5.46 (BSD-2) on libmagic; its database is
# /usr/share/file/magic.mgc (BSD/lib/libmagic).

SUBDIR =
MK_WERROR = no
MAN =
.PATH: ${ROOT_SOURCE_DIR}/BSD/lib/libmagic/src
PROG = file
SRCS = file.c
MAGICLIB = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmagic

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DHAVE_CONFIG_H -DMAGIC='"/usr/share/file/magic"' \
	-I${MAGICLIB} -I${ROOT_SOURCE_DIR}/BSD/lib/libmagic \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libmagic/src -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = ${MAGICLIB}/libmagic.a ${MINKAR_SDKROOT}/usr/lib/libz.a

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
