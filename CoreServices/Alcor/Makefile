# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Alcor, Atlas's text editor: plain text documents in windows, opened
# from Argos or `open` through LaunchServices (it declares plain text).
# Installed as /System/Applications/Alcor.app.

SUBDIR =
APP =		Alcor
APP_INSTALL_DIR = /System/Applications
SRCS =		main.m AlcorDocumentWindow.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =	${.CURDIR}/es.lproj

.include <minkar.app.mk>
