# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# man and manpath: FreeBSD main's usr.bin/man, the shell script macOS
# also uses; mandoc (../mandoc) formats the pages. The macOS layer: the
# machine comes from uname -m (Darwin has no hw.machine_arch).

SUBDIR =
MAN =

all: to-sysroot

to-sysroot: .PHONY
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/man/man5
	install -m 0555 ${.CURDIR}/man.sh ${SYSROOT_DIR}/usr/bin/man
	ln -f ${SYSROOT_DIR}/usr/bin/man ${SYSROOT_DIR}/usr/bin/manpath
	install -m 0444 ${.CURDIR}/man.1 ${.CURDIR}/manpath.1 \
	    ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${.CURDIR}/man.conf.5 ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>
