# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# open: our own, in /usr/bin as on the reference platform, over
# LaunchServices (LSOpenFromURLSpec, LSRevealInFiles).

SUBDIR =
WARNS ?= 2
PROG =	open
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreServices -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/open
	install -m 0444 ${.CURDIR}/open.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
