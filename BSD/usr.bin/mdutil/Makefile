# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mdutil: our own, in /usr/bin as on the reference platform: the metadata
# index's state of a volume (there is no index yet: indexing is off).

SUBDIR =
WARNS ?= 2
PROG =	mdutil
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/mdutil
	install -C -m 0444 ${.CURDIR}/mdutil.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
