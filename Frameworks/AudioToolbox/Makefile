# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AudioToolbox.framework: system sounds (there is no audio output yet).

MK_WERROR = no
FRAMEWORK = AudioToolbox
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AudioServices.c
HEADERS = AudioToolbox.h AudioServices.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} 
LDFLAGS += -F${FMWKS} -framework CoreFoundation

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
