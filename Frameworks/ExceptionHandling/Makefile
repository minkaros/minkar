# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ExceptionHandling.framework: NSExceptionHandler, how a program logs and
# handles the exceptions nobody catches.

SUBDIR =
MK_WERROR = no
FRAMEWORK = ExceptionHandling
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = NSExceptionHandler.m
HEADERS = NSExceptionHandler.h ExceptionHandling.h

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -fobjc-arc -F${FMWKS} -Wno-nullability-completeness
LDFLAGS += -F${FMWKS} -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
