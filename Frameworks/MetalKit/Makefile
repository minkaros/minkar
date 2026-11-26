# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MetalKit.framework: MTKView, a view backed by a CAMetalLayer drawing at
# a frame rate for its delegate. Without a Metal device (until the GPU
# track brings one) it has no drawable and its delegate still draws, as on
# the reference without one.

SUBDIR =
MK_WERROR = no
FRAMEWORK = MetalKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = MTKView.m
HEADERS = MetalKit.h MTKDefines.h MTKView.h

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework AppKit -framework Metal -framework QuartzCore -framework CoreGraphics \
	-framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.framework.mk>
