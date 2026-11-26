# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Setup (I5): a system the Installer put on a disk starts with no account;
# loginwindow runs this before anyone logs in. It asks the language, the
# keyboard, the time zone, the computer's name and the first administrator,
# and says whom to log in.

SUBDIR =
APP =		Setup
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m
FRAMEWORKS =	AppKit Foundation Carbon SystemConfiguration
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/es.lproj

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.app.mk>
