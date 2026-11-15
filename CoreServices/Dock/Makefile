# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The Dock (Atlas.md, D1): the desktop picture behind every window and
# the Dock at the bottom of the main display, with the persistent apps and
# the running ones. loginwindow starts it (SessionApplications).

SUBDIR =
APP =		Dock
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m DockModel.m DockView.m DesktopWindow.m NotificationCenterUser.c
FRAMEWORKS =	AppKit CoreGraphics Foundation
MK_WERROR =	no
CFLAGS +=	-F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks -I${.OBJDIR} -I${NC}
LDFLAGS +=	-lobjc -F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -framework SkyLight
# The Spanish texts (the English ones are the keys).
RESOURCES =	${.CURDIR}/es.lproj

# notificationd's interface, the client side: the apps' badges (N2).
NC =		${ROOT_SOURCE_DIR}/Frameworks/Foundation/NSUserNotification
CLEANFILES +=	NotificationCenterUser.c NotificationCenterUser.h
NotificationCenterUser.c NotificationCenterUser.h: ${NC}/NotificationCenter.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${NC} \
	    -user NotificationCenterUser.c -header NotificationCenterUser.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}
DockModel.o: NotificationCenterUser.h

.include <minkar.app.mk>
