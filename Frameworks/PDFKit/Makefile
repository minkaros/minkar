# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# PDFKit.framework: PDF documents, their pages, outline, destinations and
# actions, over CoreGraphics' PDF reading. Quartz re-exports it.

SUBDIR =
MK_WERROR = no
FRAMEWORK = PDFKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = PDFAction.m PDFDestination.m PDFDocument.m PDFOutline.m PDFPage.m PDFSelection.m
HEADERS = PDFKit.h PDFKitPlatform.h PDFAction.h PDFActionGoTo.h PDFActionNamed.h \
	PDFActionURL.h PDFDestination.h PDFDocument.h PDFOutline.h PDFPage.h PDFSelection.h

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework AppKit -framework CoreGraphics -framework Onyx2D -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
