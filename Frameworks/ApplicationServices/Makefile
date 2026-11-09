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
SRCS = ApplicationServices.c Processes.c
HEADERS = ApplicationServices.h Processes.h UniversalAccess.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
LDFLAGS += -F${FMWKS} -Wl,-reexport_framework,CoreServices -Wl,-reexport_framework,CoreGraphics \
	-Wl,-reexport_framework,CoreText -framework CoreFoundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
