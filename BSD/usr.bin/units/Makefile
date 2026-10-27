# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# units: FreeBSD main's usr.bin/units, unchanged, with its line editing
# through libedit. The macOS layer: macOS reads /usr/share/misc/units.lib,
# so that name is a link to FreeBSD's definitions.units.

SUBDIR =
MK_WERROR = no
PROG =	units
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-ledit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/misc \
	    ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/units
	install -m 0444 ${.CURDIR}/definitions.units ${SYSROOT_DIR}/usr/share/misc/
	ln -sf definitions.units ${SYSROOT_DIR}/usr/share/misc/units.lib
	install -m 0444 ${.CURDIR}/units.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
