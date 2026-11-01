# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# launchservicesd, LaunchServices' session daemon (WindowServer plan,
# WS4a), in /System/Library/CoreServices as on the reference platform.
# Its interface is in Frameworks/CoreServices/LaunchServices.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	launchservicesd
SRCS =	launchservicesd.c LSSessionServer.c
MAN =
LS = ${ROOT_SOURCE_DIR}/Frameworks/CoreServices/LaunchServices
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -I${.OBJDIR} -I${LS}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm
CLEANFILES += LSSessionServer.c LSSessionServer.h

# The server side of the MIG interface.
LSSessionServer.c LSSessionServer.h: ${LS}/LSSession.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${LS} \
	    -header /dev/null -user /dev/null -server LSSessionServer.c \
	    -sheader LSSessionServer.h ${.ALLSRC}

launchservicesd.o: LSSessionServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/System/Library/CoreServices
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/System/Library/CoreServices/${PROG}

.include <bsd.prog.mk>
