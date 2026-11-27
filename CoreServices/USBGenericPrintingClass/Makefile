# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The USB printer class driver CUPS's usb backend loads (backend/usb-darwin.c:
# /System/Library/Printers/Libraries/USBGenericPrintingClass.plugin, a
# CFPlug-In of the printer class type): bulk data to the printer and its
# answers back, the printer class requests (GET_DEVICE_ID,
# GET_PORT_STATUS, SOFT_RESET), all through IOUSBLib's interface.

SUBDIR =
BUNDLE =		USBGenericPrintingClass
BUNDLE_EXTENSION =	plugin
BUNDLE_INSTALL_DIR =	/System/Library/Printers/Libraries
SRCS =			USBGenericPrintingClass.c
FRAMEWORKS =		CoreFoundation IOKit
MK_WERROR =		no
WARNS ?=		2
CFLAGS +=		-std=gnu17 -Wno-nullability-completeness -Wno-incompatible-sysroot

.include <minkar.bundle.mk>
