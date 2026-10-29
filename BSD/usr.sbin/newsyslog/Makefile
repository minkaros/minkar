# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# newsyslog: FreeBSD main's usr.sbin/newsyslog, in /usr/sbin as on macOS,
# with our /etc/newsyslog.conf: the flat log files Minkar's daemons write
# (system.log and the ASL store are aslmanager's) and /etc/newsyslog.d/.
# launchd runs it every half hour (org.minkar.newsyslog). Changes for
# Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	newsyslog
SRCS =	newsyslog.c ptimes.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man8 \
	    ${SYSROOT_DIR}/usr/share/man/man5 ${SYSROOT_DIR}/private/etc/newsyslog.d
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/sbin/newsyslog
	install -m 0644 -o 0 -g 0 ${.CURDIR}/newsyslog.conf.minkar \
	    ${SYSROOT_DIR}/private/etc/newsyslog.conf
	install -m 0444 ${.CURDIR}/newsyslog.8 ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0444 ${.CURDIR}/newsyslog.conf.5 ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>
