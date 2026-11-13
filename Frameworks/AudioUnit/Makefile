# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AudioUnit.framework: audio units and components. Their functions are in
# AudioToolbox (AU3), which this framework re-exports, as the reference
# platform's does; it adds the version symbols.

MK_WERROR = no
FRAMEWORK = AudioUnit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AudioUnitVersion.c
HEADERS = AudioUnit.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation -Wl,-reexport_framework,AudioToolbox

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
