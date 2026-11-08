# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gallery: a development app with every AppKit control, the standard
# menus and panels, and a report of what AppKit lacks (classes, methods,
# menu actions nothing implements). For test images only, not for releases:
# 'bmake gallery' before 'bmake image' installs it in
# /Applications/Developer/Gallery.app. 'Gallery -report' prints the report
# and exits; 'Gallery -GalleryTab N', or the GalleryTab default before
# 'open', opens on tab N (for screendumps).

SUBDIR =
APP =		Gallery
APP_INSTALL_DIR = /Applications/Developer
SRCS =		main.m GalleryReport.m
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc

.include <minkar.app.mk>
