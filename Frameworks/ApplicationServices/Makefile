# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ApplicationServices.framework: the umbrella of CoreGraphics and CoreText
# (AppKit's headers import it, as on the reference platform). Its library
# re-exports theirs.

MK_WERROR = no
FRAMEWORK = ApplicationServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = ApplicationServices.c Processes.c Pasteboard.c AXTextMarker.c
HEADERS = ApplicationServices.h Processes.h UniversalAccess.h Pasteboard.h AXTextMarker.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
# PrintCore (built before this one) lives in this framework's Frameworks
# directory; ApplicationServices re-exports it.
PC_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/PrintCore.framework/Versions/A/PrintCore
CFLAGS += -F${MINKAR_SDKROOT}/System/Library/Frameworks/ApplicationServices.framework/Frameworks
LDFLAGS += -F${FMWKS} -Wl,-reexport_framework,CoreServices -Wl,-reexport_framework,CoreGraphics \
	-Wl,-reexport_framework,CoreText -Wl,-reexport_library,${PC_DYLIB} -framework CoreFoundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
