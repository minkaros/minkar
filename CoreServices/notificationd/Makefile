# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# notificationd, the notification center's session service (Atlas.md,
# 5.6, N0). Its interface is in Frameworks/Foundation/NSUserNotification,
# with the client (NSUserNotificationCenter).

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	notificationd
SRCS =	notificationd.c NotificationCenterServer.c
MAN =
NC = ${ROOT_SOURCE_DIR}/Frameworks/Foundation/NSUserNotification
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -Wno-nullability-completeness -I${.OBJDIR} -I${NC}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm -framework CoreFoundation
CLEANFILES += NotificationCenterServer.c NotificationCenterServer.h

# The server side of the MIG interface.
NotificationCenterServer.c NotificationCenterServer.h: ${NC}/NotificationCenter.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${NC} \
	    -header /dev/null -user /dev/null -server NotificationCenterServer.c \
	    -sheader NotificationCenterServer.h ${.ALLSRC}

notificationd.o: NotificationCenterServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <bsd.prog.mk>
