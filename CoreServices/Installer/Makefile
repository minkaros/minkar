# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Installer (I4): the installation medium's one application (loginwindow
# starts it there, as root): it checks the computer, erases the disk chosen
# and copies the medium's system to it, compressed, and makes it start.

SUBDIR =
APP =		Installer
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m
FRAMEWORKS =	AppKit Foundation DiskArbitration IOKit
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/es.lproj

.include <minkar.app.mk>
