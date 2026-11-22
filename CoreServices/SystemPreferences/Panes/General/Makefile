# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System Preferences' General pane: appearance, accent and highlight
# colors, scroll bars, the default web browser, documents.

SUBDIR =
BUNDLE =		General
BUNDLE_EXTENSION =	prefPane
BUNDLE_INSTALL_DIR =	/System/Library/PreferencePanes
SRCS =			GeneralPane.m
FRAMEWORKS =		Cocoa PreferencePanes CoreServices
MK_WERROR =		no
CFLAGS +=		-F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=		-lobjc -F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks
# The Spanish texts (the English ones are the keys).
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
