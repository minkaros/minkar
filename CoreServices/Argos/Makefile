# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Argos, the file manager (Dock-Argos.md, K1): windows that browse the file
# system with a sidebar, icon and list views, navigation and opening.
# loginwindow starts it (SessionApplications).

SUBDIR =
APP =		Argos
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m ArgosItem.m ArgosIcons.m ArgosBrowserView.m ArgosSidebarView.m \
		ArgosWindowController.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks
LDFLAGS +=	-lobjc

.include <minkar.app.mk>
