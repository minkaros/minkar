# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# distnoted, the notification center's session service (Atlas.md,
# 5.6, N0). Its interface is in Frameworks/Foundation/NSUserNotification,
# with the client (NSUserNotificationCenter).

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	distnoted
SRCS =	distnoted.c DistributedNotificationsServer.c
MAN =
NC = ${ROOT_SOURCE_DIR}/Frameworks/Foundation/NSNotificationCenter
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -Wno-nullability-completeness -I${.OBJDIR} -I${NC}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm -framework CoreFoundation
CLEANFILES += DistributedNotificationsServer.c DistributedNotificationsServer.h

# The server side of the MIG interface.
DistributedNotificationsServer.c DistributedNotificationsServer.h: ${NC}/DistributedNotifications.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${NC} \
	    -header /dev/null -user /dev/null -server DistributedNotificationsServer.c \
	    -sheader DistributedNotificationsServer.h ${.ALLSRC}

distnoted.o: DistributedNotificationsServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

.include <bsd.prog.mk>
