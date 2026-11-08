# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Sample: the template Atlas's apps start from (README.md). It builds as
# any app of the tree; 'bmake atlas-app-template' puts it in
# /Applications/Developer for test images.

SUBDIR =
APP =		Sample
APP_INSTALL_DIR = /Applications/Developer
SRCS =		main.m SampleAppDelegate.m SampleDocument.m SamplePreferences.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/en.lproj ${.CURDIR}/es.lproj ${.CURDIR}/Credits.rtf

.include <minkar.app.mk>
