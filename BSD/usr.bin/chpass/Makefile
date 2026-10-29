# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# chpass, chfn, chsh: FreeBSD main's usr.bin/chpass, in /usr/bin as on
# macOS (setuid root, editing /etc/master.passwd through libpw and
# pwd_mkdb), without NIS; changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	chpass
SRCS =	chpass.c edit.c field.c table.c util.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libpw -include pw_compat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libpw -lpw \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 4555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/chpass
.for l in chfn chsh
	ln -sf chpass ${SYSROOT_DIR}/usr/bin/${l}
	ln -sf chpass.1 ${SYSROOT_DIR}/usr/share/man/man1/${l}.1
.endfor
	install -m 0444 ${.CURDIR}/chpass.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
