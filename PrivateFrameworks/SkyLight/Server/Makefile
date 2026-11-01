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
SRCS =	main.m WSConnection.m WSDisplay.m WSDisplayServices.m WSWindowList.m \
	WSSurface.m WSCompositor.m WSInput.m WSEvents.m WSTaps.m WSKeymap.c \
	WindowServerServer.c
MAN =
SKYLIGHT = ${.CURDIR}/..
# The keymap (WSKeymap, over libxkbcommon) is the legacy server's for now;
# both move here when the legacy server goes (WS4).
LEGACY = ${ROOT_SOURCE_DIR}/CoreServices/WindowServer/Legacy
LEGACY_OBJ = ${ROOT_BINARY_DIR}${LEGACY}
.PATH: ${LEGACY}
RESOURCES = /System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fobjc-arc -Wno-nullability-completeness -Wno-incompatible-sysroot \
	-I${.OBJDIR} -I${SKYLIGHT} -I${SKYLIGHT}/IPC -I${MINKAR_SDKROOT}/usr/include/pixman-1 \
	-I${LEGACY} -I${LEGACY}/libxkbcommon/include
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework Foundation -framework IOKit -framework CoreFoundation -lbsm -lobjc \
	-lpixman-1 -L${LEGACY_OBJ}/libxkbcommon -lxkbcommon
CLEANFILES += WindowServerServer.c WindowServerServer.h

# The server side of the MIG interface.
WindowServerServer.c WindowServerServer.h: ${SKYLIGHT}/IPC/WindowServer.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} \
	    -header /dev/null -user /dev/null -server WindowServerServer.c \
	    -sheader WindowServerServer.h ${.ALLSRC}

main.o WSDisplayServices.o WSWindowList.o WSCompositor.o WSEvents.o WSTaps.o: WindowServerServer.h

# Signed with its entitlements; the image build puts it in the trust cache
# (Docs/ADR/0001-Kernel-Entitlements.md).
CODESIGN = python3 -B ${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/codesign/codesign.py

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${RESOURCES}
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${RESOURCES}/WindowServer
	${CODESIGN} -f -s - -i org.minkar.WindowServer \
	    --entitlements ${.CURDIR}/WindowServer.entitlements \
	    ${SYSROOT_DIR}${RESOURCES}/WindowServer

.include <bsd.prog.mk>
