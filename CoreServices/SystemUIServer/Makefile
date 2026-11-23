# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SystemUIServer (Atlas.md, MB1): the Menu Extras on the right of the menu
# bar, as status items of this agent app, and the notification center's
# banners and list (5.6, N0 and N1). loginwindow starts it (SessionApplications).
#
# The Menu Extras are .menu bundles built from MenuExtras/ and loaded from
# /System/Library/CoreServices/Menu Extras; the notification center stays
# built in, sharing the banners' service.

SUBDIR =
APP =		SystemUIServer
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m NotificationBanner.m NotificationSettings.m \
		NotificationCenterExtra.m NotificationCenterUser.c SystemHotKeys.m
FRAMEWORKS =	AppKit Carbon Foundation
MK_WERROR =	no
CFLAGS +=	-F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks \
		-I${.OBJDIR} -I${NC}
LDFLAGS +=	-lobjc -F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -framework SkyLight
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
