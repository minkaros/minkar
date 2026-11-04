# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The Dock (Dock-Kraz.md, D1): the desktop picture behind every window and
# the Dock at the bottom of the main display, with the persistent apps and
# the running ones. loginwindow starts it (SessionApplications).

SUBDIR =
APP =		Dock
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m DockModel.m DockView.m DesktopWindow.m
FRAMEWORKS =	AppKit CoreGraphics Foundation
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks
LDFLAGS +=	-lobjc

.include <minkar.app.mk>
