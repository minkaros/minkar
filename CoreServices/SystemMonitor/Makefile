# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Monitor (Atlas, A3), in /System/Applications/Utilities.

SUBDIR =
APP =		SystemMonitor
APP_INSTALL_DIR = /System/Applications/Utilities
SRCS =		main.m SMAppDelegate.m SMWindowController.m SMSampler.m SMGraphView.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/en.lproj ${.CURDIR}/es.lproj ${.CURDIR}/SystemMonitorHelp.help \
		${ROOT_SOURCE_DIR}/SystemLibrary/Icons/png/apps/SystemMonitor.png

.include <minkar.app.mk>
