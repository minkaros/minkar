# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# atrun: FreeBSD main's libexec/atrun, which runs at's jobs; it opens a
# PAM session for each job's user. launchd starts it every 30 seconds
# (SystemLibrary/LaunchDaemons/org.minkar.atrun.plist), disabled as on macOS until "launchctl load -w".

.include "${ROOT_SOURCE_DIR}/BSD/usr.bin/at/Makefile.inc"

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	atrun
SRCS =	atrun.c gloadavg.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness ${AT_CFLAGS} -DPAM \
	-I${ROOT_SOURCE_DIR}/BSD/usr.bin/at -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lpam
CLEANFILES += atrun.8

all: to-sysroot

atrun.8: ${.CURDIR}/atrun.man
	${AT_SED} < ${.ALLSRC} > ${.TARGET}

to-sysroot: ${PROG} atrun.8
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/atrun
	install -m 0444 atrun.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>
