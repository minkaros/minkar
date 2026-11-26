# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Carbon.framework: CoreServices and ApplicationServices re-exported, and the parts of the classic toolbox still in use.

MK_WERROR = no
FRAMEWORK = Carbon
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = HIToolbox.c HIShape.c HITheme.c TextInputSources.c
HEADERS = Carbon.h HIToolbox.h HIShape.h HITheme.h MacWindows.h Events.h TextInputSources.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -I${MINKAR_SDKROOT}/usr/include/pixman-1
LDFLAGS += -F${FMWKS} -Wl,-reexport_framework,CoreServices -Wl,-reexport_framework,ApplicationServices -framework CoreFoundation -framework CoreGraphics -lpixman-1

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.framework.mk>
