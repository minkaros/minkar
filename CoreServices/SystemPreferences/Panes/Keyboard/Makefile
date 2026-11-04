# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Preferences' Keyboard pane (ROADMAP Phase 5, item 6).

SUBDIR =
BUNDLE =		Keyboard
BUNDLE_EXTENSION =	prefPane
BUNDLE_INSTALL_DIR =	/System/Library/PreferencePanes
SRCS =			KeyboardPane.m
FRAMEWORKS =		Cocoa PreferencePanes
MK_WERROR =		no
CFLAGS +=		-F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=		-lobjc -F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -framework SkyLight
# The Spanish texts (the English ones are the keys).
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
