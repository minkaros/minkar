# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SystemUIServer (Menus.md, MB1): the Menu Extras on the right of the menu
# bar, as status items of this agent app. loginwindow starts it
# (SessionApplications).
#
# SHORTCUT(MB1): the extras are built in; MB1 moves each one to a .menu
# bundle that SystemUIServer loads.

SUBDIR =
APP =		SystemUIServer
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m ClockExtra.m
FRAMEWORKS =	AppKit Foundation
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks
LDFLAGS +=	-lobjc

.include <minkar.app.mk>
