# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Preferences (ROADMAP Phase 5, item 6): the window of the panes
# (.prefPane bundles in /System/Library/PreferencePanes and
# /Library/PreferencePanes), as an icon grid; each pane shown in place.
# Installed as /System/Applications/System Preferences.app. Its panes
# (Panes/) are built from CoreServices/Makefile.

SUBDIR =
APP =		SystemPreferences
APP_INSTALL_NAME = System Preferences
APP_INSTALL_DIR = /System/Applications
SRCS =		main.m PrefsWindowController.m
FRAMEWORKS =	Cocoa PreferencePanes
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =	${.CURDIR}/es.lproj

.include <minkar.app.mk>
