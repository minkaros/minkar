# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The TextInput Menu Extra (Atlas.md, MB1): a .menu bundle SystemUIServer loads.

SUBDIR =
BUNDLE =		TextInput
BUNDLE_EXTENSION =	menu
BUNDLE_INSTALL_DIR =	/System/Library/CoreServices/Menu Extras
SRCS =			TextInputExtra.m
FRAMEWORKS =		AppKit Foundation Carbon
MK_WERROR =		no
CFLAGS +=		-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness \
			-I${.CURDIR}/../..
LDFLAGS +=		-lobjc
RESOURCES =		${.CURDIR}/es.lproj

.include <minkar.bundle.mk>
