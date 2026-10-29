# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mtree: FreeBSD main's (NetBSD's mtree, contrib/mtree, with pack_dev.c
# from contrib/mknod), in /usr/sbin as on macOS; changes for Darwin are
# the lines marked Minkar. The digests are MD5, SHA-1 and SHA-2 from our
# libmd; RIPEMD-160 is left out (NO_RMD160), as in that library.
# libnetbsd's util.h has the same name as Darwin's, which declares
# fparseln (in libc): Darwin's is read first.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	mtree
SRCS =	compare.c crc.c create.c excludes.c getid.c misc.c mtree.c \
	only.c spec.c specspec.c verify.c pack_dev.c
# NetBSD's helpers (emalloc, flags_to_string...), from FreeBSD's libnetbsd.
.PATH: ${ROOT_SOURCE_DIR}/BSD/lib/libnetbsd
SRCS +=	efun.c util.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DNO_RMD160 -D_DARWIN_C_SOURCE \
	-D__va_list=__builtin_va_list -include ${MINKAR_SDKROOT}/usr/include/util.h -include sys/time.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libnetbsd -I${ROOT_SOURCE_DIR}/BSD/lib/libmd \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man5 \
	    ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/mtree
	install -m 0444 ${.CURDIR}/mtree.5 ${SYSROOT_DIR}/usr/share/man/man5/
	install -m 0444 ${.CURDIR}/mtree.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
