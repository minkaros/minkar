# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# iosurfaced, the IOSurface registry (roadmap step 4): global IDs, memory
# entries and values of surfaces. Its interface is in Frameworks/IOSurface.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	iosurfaced
SRCS =	iosurfaced.c IOSurfaceRegistryServer.c
MAN =
IOS = ${ROOT_SOURCE_DIR}/Frameworks/IOSurface
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -I${.OBJDIR} -I${IOS}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm
CLEANFILES += IOSurfaceRegistryServer.c IOSurfaceRegistryServer.h

# The server side of the MIG interface.
IOSurfaceRegistryServer.c IOSurfaceRegistryServer.h: ${IOS}/IOSurfaceRegistry.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${IOS} \
	    -header /dev/null -user /dev/null -server IOSurfaceRegistryServer.c \
	    -sheader IOSurfaceRegistryServer.h ${.ALLSRC}

iosurfaced.o: IOSurfaceRegistryServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

.include <bsd.prog.mk>
