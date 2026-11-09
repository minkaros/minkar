# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Logs (Atlas, A4), in /System/Applications/Utilities.

SUBDIR =
APP =		Logs
APP_INSTALL_DIR = /System/Applications/Utilities
SRCS =		main.m LGAppDelegate.m LGWindowController.m LGLogReader.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/en.lproj ${.CURDIR}/es.lproj ${.CURDIR}/LogsHelp.help \
		${ROOT_SOURCE_DIR}/SystemLibrary/Icons/png/apps/Logs.png

.include <minkar.app.mk>
