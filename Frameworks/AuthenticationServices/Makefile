# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AuthenticationServices.framework: signing in through the system (single
# sign-on providers, web authentication sessions handed to the browser).
# Minkar has no sign-on extensions and no broker for web authentication
# sessions yet: requests are not handled, and none reach a browser.

MK_WERROR = no
FRAMEWORK = AuthenticationServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = ASAuthorization.m
HEADERS = AuthenticationServices.h ASAuthorization.h
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
