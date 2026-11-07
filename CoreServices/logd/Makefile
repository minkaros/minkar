# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# logd (Phase 3): keeps os_log's messages. libsystem_trace sends them
# (logd_message.h); the log command reads them through logd.defs.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	logd
SRCS =	logd.c logdServer.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -I${.OBJDIR} -I${.CURDIR} \
	-I${MINKAR_SDKROOT}/usr/local/include
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm
CLEANFILES += logdServer.c logdServer.h

# The server side of the MIG interface.
logdServer.c logdServer.h: ${.CURDIR}/logd.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${.CURDIR} \
	    -header /dev/null -user /dev/null -server logdServer.c \
	    -sheader logdServer.h ${.ALLSRC}

logd.o: logdServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${MINKAR_SDKROOT}/usr/local/include/minkar
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}
	install -d -m 0755 -o 0 -g 0 ${SYSROOT_DIR}/private/var/log/logd
	# For the log command.
	cp -f ${.CURDIR}/logd.defs ${.CURDIR}/logd_record.h ${MINKAR_SDKROOT}/usr/local/include/minkar

.include <bsd.prog.mk>
