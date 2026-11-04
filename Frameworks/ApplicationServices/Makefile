# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ApplicationServices.framework: the umbrella of CoreGraphics and CoreText
# (AppKit's headers import it, as on the reference platform). Its library
# re-exports theirs.

MK_WERROR = no
FRAMEWORK = ApplicationServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = ApplicationServices.c
HEADERS = ApplicationServices.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
LDFLAGS += -F${FMWKS} -Wl,-reexport_framework,CoreGraphics -Wl,-reexport_framework,CoreText

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rm -rf ${MINKAR_SDKROOT}/System/Library/Frameworks/${FRAMEWORK}.framework \
	  ${SYSROOT_DIR}/System/Library/Frameworks/${FRAMEWORK}.framework
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
