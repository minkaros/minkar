# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The Printers pane: CUPS's printers, added and deleted, the defaults.

SUBDIR =
BUNDLE =		Printers
BUNDLE_EXTENSION =	prefPane
BUNDLE_INSTALL_DIR =	/System/Library/PreferencePanes
SRCS =			PrintersPane.m
FRAMEWORKS =		Cocoa PreferencePanes SecurityInterface Security
MK_WERROR =		no
CFLAGS +=		-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=		-lobjc -lcups
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
