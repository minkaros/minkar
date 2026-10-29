# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# xxd: our own (BSD-2-Clause), written from the command's documented
# behaviour; macOS ships Vim's, under Vim's licence. Same options, output
# columns, C include format and -r.

SUBDIR =
WARNS ?= 3
PROG =	xxd
MAN =
BINDIR = /usr/bin
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR} ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/
	install -m 0444 ${.CURDIR}/xxd.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
