# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Argos, the file manager (Atlas.md, K1): windows that browse the file
# system with a sidebar, icon, list and column views, navigation and opening.
# loginwindow starts it (SessionApplications).

SUBDIR =
APP =		Argos
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m ArgosItem.m ArgosIcons.m ArgosBrowserView.m ArgosColumnView.m ArgosSidebarView.m \
		ArgosWindowController.m ArgosInfoWindow.m
FRAMEWORKS =	Cocoa CoreServices
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness
LDFLAGS +=	-lobjc
# The Spanish texts (the English ones are the keys), and the icon
# (CFBundleIconFile). Argos.png is a test icon, not the final one.
RESOURCES =	${.CURDIR}/es.lproj ${.CURDIR}/Argos.png

.include <minkar.app.mk>
