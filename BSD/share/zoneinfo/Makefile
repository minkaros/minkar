# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# zoneinfo: the time zones, from the IANA time zone
# database (public domain):
#   tzdata/     the zone and rule sources, zone.tab, zone1970.tab and
#               iso3166.tab (tzdata 2026e)
#   tzcode/     zic, built for the build host to compile them (tzcode 2026e)
# The files are "fat" (every transition written out, as older readers
# expect). They are laid out as on macOS since 10.13, where libc reads
# /var/db/timezone/zoneinfo:
#   /usr/share/zoneinfo.default     the files
#   /var/db/timezone/zoneinfo   ->  /usr/share/zoneinfo.default
#   /usr/share/zoneinfo         ->  /var/db/timezone/zoneinfo
#   /etc/localtime              ->  /var/db/timezone/zoneinfo/UTC, until the
#                                   system's zone is set

SUBDIR =
HOST_CC ?= cc
TZVERSION != cat ${.CURDIR}/tzdata/version
ZONES =	africa antarctica asia australasia europe northamerica southamerica \
	etcetera backward factory
TABLES = zone.tab zone1970.tab iso3166.tab

all: to-sysroot

zic: ${.CURDIR}/tzcode/zic.c ${.CURDIR}/tzcode/private.h ${.CURDIR}/tzcode/tzfile.h
	printf '%s\n' 'static char const PKGVERSION[] = "(tzcode) ";' \
	    'static char const TZVERSION[] = "${TZVERSION}";' \
	    'static char const REPORT_BUGS_TO[] = "tz@iana.org";' > version.h
	printf '%s\n' '#define TZDEFAULT "/etc/localtime"' \
	    '#define TZDIR "/usr/share/zoneinfo"' > tzdir.h
	${HOST_CC} -O2 -I${.OBJDIR} -I${.CURDIR}/tzcode -o ${.TARGET} \
	    ${.CURDIR}/tzcode/zic.c

to-sysroot: zic .PHONY
	rm -rf ${.OBJDIR}/out
	cd ${.CURDIR}/tzdata && ${.OBJDIR}/zic -b fat -d ${.OBJDIR}/out ${ZONES}
	cd ${.CURDIR}/tzdata && cp ${TABLES} ${.OBJDIR}/out/
	echo ${TZVERSION} > ${.OBJDIR}/out/+VERSION
	rm -rf ${SYSROOT_DIR}/usr/share/zoneinfo ${SYSROOT_DIR}/usr/share/zoneinfo.default
	mkdir -p ${SYSROOT_DIR}/usr/share/zoneinfo.default \
	    ${SYSROOT_DIR}/private/var/db/timezone ${SYSROOT_DIR}/private/etc
	cd ${.OBJDIR}/out && cp -R . ${SYSROOT_DIR}/usr/share/zoneinfo.default/
	ln -sfn /usr/share/zoneinfo.default \
	    ${SYSROOT_DIR}/private/var/db/timezone/zoneinfo
	ln -sfn /var/db/timezone/zoneinfo ${SYSROOT_DIR}/usr/share/zoneinfo
	[ -L ${SYSROOT_DIR}/private/etc/localtime ] || \
	    ln -sf /var/db/timezone/zoneinfo/UTC ${SYSROOT_DIR}/private/etc/localtime
