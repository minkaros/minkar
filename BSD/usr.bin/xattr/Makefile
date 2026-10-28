# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# xattr: macOS's extended attribute tool, written for Minkar.

SUBDIR =
PROG=	xattr
MAN =
MK_WERROR = no

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/xattr

.include <bsd.prog.mk>
