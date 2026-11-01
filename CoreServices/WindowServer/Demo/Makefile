# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# WindowServerDemo: the demo client of the WindowServer plan's WS0. One
# window that the boottest gate M4-draw draws and sends a key and a click
# to; it prints the frame and every key and mouse event it receives.

APP = WindowServerDemo
SUBDIR =
APP_INSTALL_DIR = /System/Library/CoreServices
MK_WERROR = no
WARNS = 1

SRCS = main.m

FRAMEWORKS = AppKit Foundation

# Some SDK headers are not self-contained yet (see ../Makefile): take
# them from the source tree, as WindowServer does.
CFLAGS += -fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot \
	-I${ROOT_SOURCE_DIR}/Frameworks -I${ROOT_SOURCE_DIR}/CoreServices
LDFLAGS += -lobjc

.include <minkar.app.mk>
