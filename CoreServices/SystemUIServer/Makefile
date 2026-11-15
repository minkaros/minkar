# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SystemUIServer (Atlas.md, MB1): the Menu Extras on the right of the menu
# bar, as status items of this agent app, and the notification center's
# banners (5.6, N0). loginwindow starts it (SessionApplications).
#
# SHORTCUT(MB1): the extras are built in; MB1 moves each one to a .menu
# bundle that SystemUIServer loads.

SUBDIR =
APP =		SystemUIServer
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m ClockExtra.m VolumeExtra.m NotificationBanner.m NotificationCenterUser.c
FRAMEWORKS =	AppKit Foundation CoreAudio
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks \
		-I${.OBJDIR} -I${NC}
LDFLAGS +=	-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =	${.CURDIR}/es.lproj

# notificationd's interface, the client side (Foundation has the apps').
NC =		${ROOT_SOURCE_DIR}/Frameworks/Foundation/NSUserNotification
CLEANFILES +=	NotificationCenterUser.c NotificationCenterUser.h
NotificationCenterUser.c NotificationCenterUser.h: ${NC}/NotificationCenter.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${NC} \
	    -user NotificationCenterUser.c -header NotificationCenterUser.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}
NotificationBanner.o: NotificationCenterUser.h

.include <minkar.app.mk>
