# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# getconf: FreeBSD main's usr.bin/getconf. Its name tables are .gperf
# files turned into C by fake-gperf.awk, as FreeBSD builds them; the names
# Darwin lacks are left out by the #ifdefs they already carry.

SUBDIR =
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
PROG =	getconf
SRCS =	confstr.c getconf.c limits.c pathconf.c progenv.c sysconf.c \
	unsigned_limits.c
CFLAGS += -I${.CURDIR}
CLEANFILES += confstr.c limits.c pathconf.c progenv.c sysconf.c \
	unsigned_limits.c

.SUFFIXES: .gperf
FAKE_GPERF = ${.CURDIR}/fake-gperf.awk
.gperf.c: ${FAKE_GPERF}
	LC_ALL=C awk -f ${FAKE_GPERF} ${.IMPSRC} >${.TARGET}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -m 0444 ${.CURDIR}/getconf.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
