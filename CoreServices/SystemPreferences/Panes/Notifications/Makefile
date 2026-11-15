# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Preferences' Notifications pane (Atlas.md, 5.6, N1).

SUBDIR =
BUNDLE =		Notifications
BUNDLE_EXTENSION =	prefPane
BUNDLE_INSTALL_DIR =	/System/Library/PreferencePanes
SRCS =			NotificationsPane.m
FRAMEWORKS =		Cocoa PreferencePanes 
MK_WERROR =		no
CFLAGS +=		-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness \
			-I${ROOT_SOURCE_DIR}/Frameworks/Foundation/NSUserNotification
LDFLAGS +=		-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
