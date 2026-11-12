# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# log (Phase 3): shows, streams and configures os_log's messages, which
# logd keeps (CoreServices/logd, through its MIG interface).

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	log
SRCS =	log.m logdUser.c
LOGD =	${ROOT_SOURCE_DIR}/CoreServices/logd
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot -I${.OBJDIR} -I${LOGD}
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD =	-framework Foundation -lobjc
CLEANFILES += logdUser.c logd.h

# The client side of logd's MIG interface.
logdUser.c logd.h: ${LOGD}/logd.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${LOGD} \
	    -user logdUser.c -header logd.h -server /dev/null -sheader /dev/null ${.ALLSRC}

log.o: logd.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -C -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -C -m 0444 ${.CURDIR}/log.1 ${SYSROOT_DIR}/usr/share/man/man1/log.1

.include <bsd.prog.mk>
