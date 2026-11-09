# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# UniformTypeIdentifiers.framework: types as objects (API 20), on
# LaunchServices' type declarations.

MK_WERROR = no
FRAMEWORK = UniformTypeIdentifiers
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = UTType.m UTCoreTypes.m
HEADERS = UniformTypeIdentifiers.h UTType.h UTCoreTypes.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -I${.CURDIR} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework CoreServices -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
