# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Preferences' Network pane (ROADMAP Phase 5, item 6).

SUBDIR =
BUNDLE =		Network
BUNDLE_EXTENSION =	prefPane
BUNDLE_INSTALL_DIR =	/System/Library/PreferencePanes
SRCS =			NetworkPane.m
FRAMEWORKS =		Cocoa PreferencePanes SystemConfiguration
MK_WERROR =		no
CFLAGS +=		-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=		-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
