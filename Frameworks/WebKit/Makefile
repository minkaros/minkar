# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# WebKit.framework: the API's web view classes (WKWebView and the older
# WebView). Their engine is to be WPE WebKit (Docs/ROADMAP.md); until then
# a page is shown as its text through AppKit's HTML reader.

MK_WERROR = no
FRAMEWORK = WebKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = WebPage.m WKWebView.m WebView.m
HEADERS = WebKit.h WKWebView.h WebView.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -I${.CURDIR} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework AppKit -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
