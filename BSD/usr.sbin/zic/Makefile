# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# zic, the time zone compiler (tzcode, public domain), for the target: it
# compiles zone sources into the files in /usr/share/zoneinfo, as macOS's
# /usr/sbin/zic does. The tzfile(5) manual comes with it. (BSD/share/zoneinfo builds its
# own zic for the build host to make the image's files.)
# Sources: BSD/share/zoneinfo/tzcode, the release in tzdata/version.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
MAN =
PROG =	zic
TZCODE = ${ROOT_SOURCE_DIR}/BSD/share/zoneinfo/tzcode
TZVERSION != cat ${ROOT_SOURCE_DIR}/BSD/share/zoneinfo/tzdata/version
.PATH:	${TZCODE}
SRCS =	zic.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DUSE_LTZ=0 -I${.OBJDIR} -I${TZCODE}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}

version.h tzdir.h: ${ROOT_SOURCE_DIR}/BSD/share/zoneinfo/tzdata/version
	printf '%s\n' 'static char const PKGVERSION[] = "(tzcode) ";' \
	    'static char const TZVERSION[] = "${TZVERSION}";' \
	    'static char const REPORT_BUGS_TO[] = "tz@iana.org";' > version.h
	printf '%s\n' '#define TZDEFAULT "/etc/localtime"' \
	    '#define TZDIR "/usr/share/zoneinfo"' > tzdir.h
CLEANFILES += version.h tzdir.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/sbin/
	install -m 0444 ${TZCODE}/zic.8 ${SYSROOT_DIR}/usr/share/man/man8/
	mkdir -p ${SYSROOT_DIR}/usr/share/man/man5
	install -m 0444 ${TZCODE}/tzfile.5 ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>

${OBJS}: version.h tzdir.h
