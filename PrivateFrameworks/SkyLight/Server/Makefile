# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The new window server (WindowServer plan, WS1), installed in
# SkyLight.framework/Resources. Built in the CoreServices stage, after the
# frameworks it links.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	WindowServer
SRCS =	main.m WindowServerServer.c
MAN =
SKYLIGHT = ${.CURDIR}/..
RESOURCES = /System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fobjc-arc -Wno-nullability-completeness -Wno-incompatible-sysroot \
	-I${.OBJDIR} -I${SKYLIGHT}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework Foundation -lbsm -lobjc
CLEANFILES += WindowServerServer.c WindowServerServer.h

# The server side of the MIG interface.
WindowServerServer.c WindowServerServer.h: ${SKYLIGHT}/IPC/WindowServer.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} \
	    -header /dev/null -user /dev/null -server WindowServerServer.c \
	    -sheader WindowServerServer.h ${.ALLSRC}

main.o: WindowServerServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${RESOURCES}
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${RESOURCES}/WindowServer

.include <bsd.prog.mk>
