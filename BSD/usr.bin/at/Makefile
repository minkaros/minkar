# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# at, atq, atrm, batch: FreeBSD main's usr.bin/at (at 2.9, BSD licence:
# LEGAL), set-user-ID root as on macOS. The jobs run from atrun
# (BSD/libexec/atrun), started by launchd. Changes for Darwin are the lines
# marked Minkar.

.include "${.CURDIR}/Makefile.inc"

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	at
SRCS =	at.c panic.c parsetime.c perm.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness ${AT_CFLAGS} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
CLEANFILES += at.1

all: to-sysroot

at.1: ${.CURDIR}/at.man
	${AT_SED} < ${.ALLSRC} > ${.TARGET}

to-sysroot: ${PROG} at.1
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 4555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/at
.for l in atq atrm batch
	ln -f ${SYSROOT_DIR}/usr/bin/at ${SYSROOT_DIR}/usr/bin/${l}
.endfor
	install -m 0444 at.1 ${SYSROOT_DIR}/usr/share/man/man1/
.for l in atq atrm batch
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/at.1 ${SYSROOT_DIR}/usr/share/man/man1/${l}.1
.endfor
	# the spool, as at(1) expects it (FreeBSD's mtree: daemon, 0770)
	mkdir -p ${SYSROOT_DIR}/private/var/at/jobs ${SYSROOT_DIR}/private/var/at/spool
	chown 1:1 ${SYSROOT_DIR}/private/var/at/jobs ${SYSROOT_DIR}/private/var/at/spool
	chmod 0770 ${SYSROOT_DIR}/private/var/at/jobs ${SYSROOT_DIR}/private/var/at/spool
	touch ${SYSROOT_DIR}/private/var/at/jobs/.lockfile
	chown 1:1 ${SYSROOT_DIR}/private/var/at/jobs/.lockfile
	# an empty at.deny: every user may use at (perm.c; as FreeBSD installs it)
	install -m 0600 -o 0 -g 0 /dev/null ${SYSROOT_DIR}/private/var/at/at.deny

.include <bsd.prog.mk>
