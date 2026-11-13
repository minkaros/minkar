# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# LocalAuthentication.framework: confirming that the user is the owner of the
# machine (LAContext). SHORTCUT(19.6.0): there is no authentication dialog
# yet (see Security/minkar/Authorization.c), so no policy can be evaluated.

MK_WERROR = no
FRAMEWORK = LocalAuthentication
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = LAContext.m
HEADERS = LocalAuthentication.h LAContext.h LAError.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
