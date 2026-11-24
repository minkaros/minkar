# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Preferences' Applications pane (default apps, login items and permissions).

SUBDIR =
BUNDLE =		Applications
BUNDLE_EXTENSION =	prefPane
BUNDLE_INSTALL_DIR =	/System/Library/PreferencePanes
SRCS =			ApplicationsPane.m
FRAMEWORKS =		Cocoa PreferencePanes CoreServices
MK_WERROR =		no
CFLAGS +=		-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=		-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
