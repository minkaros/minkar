# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SecurityInterface.framework: SFAuthorization and SFAuthorizationView, the
# lock a window's settings sit behind (the maintainer's choice, 2026-11-23:
# authorization in full, the panes' lock), over Security's Authorization.

SUBDIR =
MK_WERROR = no
FRAMEWORK = SecurityInterface
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = SFAuthorizationView.m
HEADERS = SecurityInterface.h SFAuthorization.h SFAuthorizationView.h
RESOURCES_DIRS = es.lproj
RESOURCES_FILES = Info.plist

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
# Its own headers as <SecurityInterface/...>, from the framework being built.
CFLAGS += -fobjc-arc -F${.OBJDIR} -F${FMWKS} -Wno-nullability-completeness
SFAuthorizationView.o: _copy_headers
LDFLAGS += -F${FMWKS} -framework AppKit -framework Foundation -framework Security -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
