# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# /bin/sh: the launcher of the shell /private/var/select/sh names (bash,
# as POSIX sh, by default; BSD/hier/hier.spec makes the link), as on the
# reference system.

SUBDIR =
PROG =	shlauncher
MAN =

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -C -m 0555 ${PROG} ${SYSROOT_DIR}/bin/sh

.include <bsd.prog.mk>
