# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.

SUBDIR =
PROG =	locate
SRCS =	util.c locate.c
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR}/../locate \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
CFLAGS += -DMMAP
SCRIPTS = updatedb mklocatedb concatdb

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/libexec \
	    ${SYSROOT_DIR}/etc ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/locate
.for s in ${SCRIPTS}
	install -m 0555 ${.CURDIR}/${s}.sh ${SYSROOT_DIR}/usr/libexec/locate.${s}
.endfor
	install -m 0644 ${.CURDIR}/locate.rc ${SYSROOT_DIR}/etc/locate.rc
	install -m 0444 ${.CURDIR}/locate.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${.CURDIR}/locate.updatedb.8 ${SYSROOT_DIR}/usr/share/man/man8/
	ln -f ${SYSROOT_DIR}/usr/share/man/man8/locate.updatedb.8 \
	    ${SYSROOT_DIR}/usr/share/man/man8/updatedb.8

.include <bsd.prog.mk>
