# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# calendar: FreeBSD main's usr.bin/calendar, unchanged, with its
# calendars/calendar.freebsd in /usr/share/calendar (FreeBSD moved the
# other calendars to the calendar-data package).

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	calendar
SRCS =	calendar.c locale.c events.c dates.c parsedata.c io.c day.c ostern.c paskha.c pom.c sunpos.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DWITH_ICONV \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lm -liconv

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/calendar
	install -m 0444 ${.CURDIR}/calendar.1 ${SYSROOT_DIR}/usr/share/man/man1/
	mkdir -p ${SYSROOT_DIR}/usr/share/calendar
	install -m 0444 ${.CURDIR}/calendars/calendar.freebsd ${SYSROOT_DIR}/usr/share/calendar/

.include <bsd.prog.mk>
