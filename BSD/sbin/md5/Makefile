# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# md5 and the SHA digests (original, BSD-2: md5.c), on our libmd; shasum
# is the same program.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	md5
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libmd \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/sbin/
.for l in sha1 sha224 sha256 sha384 sha512 sha512t224 sha512t256
	ln -f ${SYSROOT_DIR}/sbin/md5 ${SYSROOT_DIR}/sbin/${l}
.endfor
	ln -f ${SYSROOT_DIR}/sbin/md5 ${SYSROOT_DIR}/usr/bin/shasum

.include <bsd.prog.mk>
