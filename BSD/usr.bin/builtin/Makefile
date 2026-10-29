# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# alias, bg, cd, command, fc, fg, getopts, hash, jobs, read, type, ulimit,
# umask, unalias and wait as files in /usr/bin, as on macOS: one script
# (generic.sh, our own) that runs sh's built-in of the name it was started
# by, and builtin(1) about them.

NAMES =	alias bg cd command fc fg getopts hash jobs read type ulimit umask \
	unalias wait

all: to-sysroot

to-sysroot: .PHONY
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.CURDIR}/generic.sh ${SYSROOT_DIR}/usr/bin/alias
.for n in ${NAMES:Nalias}
	ln -f ${SYSROOT_DIR}/usr/bin/alias ${SYSROOT_DIR}/usr/bin/${n}
.endfor
	install -m 0444 ${.CURDIR}/builtin.1 ${SYSROOT_DIR}/usr/share/man/man1/
.for n in ${NAMES}
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/builtin.1 \
	    ${SYSROOT_DIR}/usr/share/man/man1/${n}.1
.endfor

clean cleandir obj depend install: .PHONY
