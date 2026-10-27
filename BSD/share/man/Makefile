# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The commands' manual pages: after BSD's programs are installed,
# install-pages.sh puts in the page each command's source carries
# (NAME.1 or NAME.8), for the commands the system has, when the command's
# own Makefile did not.
# man1 ... man9 here are FreeBSD's general pages; they are not installed.

SUBDIR =

all: to-sysroot

to-sysroot: .PHONY
	sh ${.CURDIR}/install-pages.sh ${ROOT_SOURCE_DIR}/BSD ${SYSROOT_DIR}

.include <bsd.prog.mk>
