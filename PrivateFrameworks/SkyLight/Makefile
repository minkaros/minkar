# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SkyLight.framework: the window server's client side (WindowServer plan,
# WS1), in /System/Library/PrivateFrameworks. Built in the Frameworks
# stage, before CoreGraphics, which uses it. The new server (Server/) and
# its test client (Tests/) are built in the CoreServices stage; the server
# goes in this framework's Resources.

FRAMEWORK = SkyLight
FMWK_INSTALL_DIR = /System/Library/PrivateFrameworks
SUBDIR =
MK_WERROR = no

SRCS = SLSConnection.c SLSDisplay.c SLSDisplayMode.c SLSGamma.c SLSCapture.c \
	SLSWindow.c SLSSurface.c SLSEvents.c WindowServerUser.c
HEADERS = SkyLight.h
CFLAGS += -I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/IPC
CLEANFILES += WindowServerUser.c WindowServer.h

# The client side of the MIG interface.
WindowServerUser.c WindowServer.h: ${.CURDIR}/IPC/WindowServer.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} \
	    -user WindowServerUser.c -header WindowServer.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}

${SRCS:NWindowServerUser.c:S/.c$/.o/}: WindowServer.h

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}${FMWK_INSTALL_DIR} ${SYSROOT_DIR}${FMWK_INSTALL_DIR}
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}${FMWK_INSTALL_DIR}
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}${FMWK_INSTALL_DIR}

.include <minkar.framework.mk>
