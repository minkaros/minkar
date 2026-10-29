# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# zdump, which prints a time zone's transitions (tzcode, public domain), as
# macOS's /usr/sbin/zdump. It uses libc's localtime (USE_LTZ=0).
# Sources: BSD/share/zoneinfo/tzcode, the release in tzdata/version.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
MAN =
PROG =	zdump
TZCODE = ${ROOT_SOURCE_DIR}/BSD/share/zoneinfo/tzcode
TZVERSION != cat ${ROOT_SOURCE_DIR}/BSD/share/zoneinfo/tzdata/version
.PATH:	${TZCODE}
SRCS =	zdump.c

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
	install -m 0444 ${TZCODE}/zdump.8 ${SYSROOT_DIR}/usr/share/man/man8/

.include <bsd.prog.mk>

${OBJS}: version.h tzdir.h
