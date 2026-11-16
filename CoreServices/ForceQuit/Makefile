# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Force Quit (Atlas.md, MB3): the window that lists the running
# applications and forces one to quit. The system menu's Force Quit... and
# Command-Option-Escape (loginwindow) open it.

SUBDIR =
APP =		ForceQuit
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m
FRAMEWORKS =	AppKit Foundation
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/es.lproj

.include <minkar.app.mk>
