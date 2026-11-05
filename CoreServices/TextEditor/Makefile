# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Text Editor, Atlas's text editor: plain text documents in windows, opened
# from Files or `open` through LaunchServices (it declares plain text).
# Installed as /System/Applications/TextEditor.app.

SUBDIR =
APP =		TextEditor
APP_INSTALL_DIR = /System/Applications
SRCS =		main.m EditorDocumentWindow.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
# The Spanish texts (the English ones are the keys).
RESOURCES =	${.CURDIR}/es.lproj

.include <minkar.app.mk>
