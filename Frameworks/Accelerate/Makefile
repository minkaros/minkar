# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Accelerate.framework (AU9): vDSP (vector arithmetic, Fourier
# transforms, filters), vForce and the vImage conversions apps use.
# Written from the documented definitions; what IINA, VLC and VSCodium
# import, and the functions next to it.

MK_WERROR = no
FRAMEWORK = Accelerate
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = vDSP.c vForce.c vImage.c
HEADERS = Accelerate.h vDSP.h vForce.h vImage.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -O2 -I${.CURDIR}
LDFLAGS += -lm

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
