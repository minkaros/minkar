# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Carbon.framework: CoreServices and ApplicationServices re-exported, and the parts of the classic toolbox still in use.

MK_WERROR = no
FRAMEWORK = Carbon
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = HIToolbox.c TextInputSources.c
HEADERS = Carbon.h HIToolbox.h Events.h TextInputSources.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} 
LDFLAGS += -F${FMWKS} -Wl,-reexport_framework,CoreServices -Wl,-reexport_framework,ApplicationServices -framework CoreFoundation

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
