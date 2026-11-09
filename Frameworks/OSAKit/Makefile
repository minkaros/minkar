# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# OSAKit.framework: OSA scripts (there is no scripting component yet).

MK_WERROR = no
FRAMEWORK = OSAKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = OSAKit.m
HEADERS = OSAKit.h
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
