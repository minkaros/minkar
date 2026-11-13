# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Metal.framework, until the GPU track brings Metal (darling-metal and
# Indium, decided): there is no Metal device yet, so MTLCreateSystemDefaultDevice
# returns nil and MTLCopyAllDevices an empty array; the descriptors, which
# hold settings only, are complete.

MK_WERROR = no
FRAMEWORK = Metal
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = MTLDescriptors.m
HEADERS = Metal.h MTLDescriptors.h
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
