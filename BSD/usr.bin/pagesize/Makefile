# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pagesize: FreeBSD main's usr.bin/pagesize, a script over sysctl.

SUBDIR =

all: to-sysroot

to-sysroot:
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.CURDIR}/pagesize.sh ${SYSROOT_DIR}/usr/bin/pagesize
	install -m 0444 ${.CURDIR}/pagesize.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
