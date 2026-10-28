# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# make: bmake 20260704 (BSD; dist/LICENSE), the BSD make Simon J. Gerraty
# keeps from NetBSD's, as FreeBSD main ships it (contrib/bmake). dist/
# holds its sources, manual page and portable mk files, unchanged; without
# its GNU configure (config.h is FreeBSD's, which fits Darwin), unit tests
# or filemon (a FreeBSD device). macOS's make is GNU make; Minkar's is BSD
# make. Installs /usr/bin/make, bmake as a second name, make(1) and the
# mk files in /usr/share/mk (sys.mk reads sys/Darwin.mk).

SUBDIR =
MK_WERROR = no
BMAKE_VERSION =	20260704
PROG =	make
SRCS =	arch.c buf.c compat.c cond.c dir.c for.c hash.c job.c lst.c \
	main.c make.c make_malloc.c meta.c metachar.c parse.c str.c \
	suff.c targ.c trace.c util.c var.c stresep.c
MAN =
.PATH: ${.CURDIR}/dist
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${.CURDIR} -I${.CURDIR}/dist -DHAVE_CONFIG_H -DUSE_META \
	-DBMAKE_PATH_MAX=1024 -D_PATH_DEFSYSPATH=\"/usr/share/mk\" \
	-DMAKE_NATIVE -DMAKE_VERSION=\"${BMAKE_VERSION}\"
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/mk
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/make
	ln -f ${SYSROOT_DIR}/usr/bin/make ${SYSROOT_DIR}/usr/bin/bmake
	install -m 0444 ${.CURDIR}/dist/make.1 ${SYSROOT_DIR}/usr/share/man/man1/make.1
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/make.1 \
	    ${SYSROOT_DIR}/usr/share/man/man1/bmake.1
	sh ${.CURDIR}/dist/mk/install-mk -m 444 ${SYSROOT_DIR}/usr/share/mk

.include <bsd.prog.mk>
