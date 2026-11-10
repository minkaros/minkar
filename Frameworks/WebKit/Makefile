# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# WebKit.framework: the API's web view classes. WKWebView runs on WPE
# WebKit (BSD/lib/wpewebkit, private in /usr/lib/webkit) through Minkar's
# WPE platform, so this framework is built after BSD, in a stage of its
# own (the top Makefile). The older WebView shows a WKWebView of its own.

MK_WERROR = no
FRAMEWORK = WebKit
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = WKWebView.m WebView.m DOM.m WebScriptBridge.m WebGLibRunLoop.m WebWPEPlatform.m WebWPEEvents.m
HEADERS = WebKit.h WKWebView.h WebView.h DOM.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
# WPE WebKit's flags, from its pkg-config files in the SDK (private: under
# usr/local/webkit), with the SDK as sysroot.
WPE_PKG_ENV =	env PKG_CONFIG_PATH= PKG_CONFIG_SYSROOT_DIR=${MINKAR_SDKROOT} PKG_CONFIG_FDO_SYSROOT_RULES=1 \
		PKG_CONFIG_LIBDIR=${MINKAR_SDKROOT}/usr/local/webkit/lib/pkgconfig:${MINKAR_SDKROOT}/usr/lib/pkgconfig:${MINKAR_SDKROOT}/usr/local/lib/pkgconfig
WPE_MODULES =	wpe-webkit-2.0 wpe-platform-2.0
.if make(all) || make(install) || empty(.TARGETS)
WPE_CFLAGS !=	${WPE_PKG_ENV} pkg-config --cflags ${WPE_MODULES} || echo WPE-WEBKIT-MISSING
WPE_LIBS !=	${WPE_PKG_ENV} pkg-config --libs ${WPE_MODULES} || echo WPE-WEBKIT-MISSING
.if ${WPE_CFLAGS:MWPE-WEBKIT-MISSING} || ${WPE_LIBS:MWPE-WEBKIT-MISSING}
.error WebKit.framework: WPE WebKit is not in the SDK (build BSD/lib/wpewebkit first)
.endif
.endif
CFLAGS += -I${.CURDIR:H} -I${.CURDIR} -F${FMWKS} -fobjc-arc ${WPE_CFLAGS}
LDFLAGS += -F${FMWKS} -framework AppKit -framework CoreGraphics -framework Foundation -lobjc ${WPE_LIBS}

# The web process extension (WebScriptingExtension.c): a module WebKit's
# web processes load, in the framework (WebScriptBridge.m tells WebKit
# where).
EXTENSION_DIR = ${.OBJDIR}/${FRAMEWORK}.framework/Versions/A/WebProcessExtensions
EXTENSION = ${EXTENSION_DIR}/libMinkarWebScripting.so
.if make(all) || make(install) || empty(.TARGETS)
EXTENSION_FLAGS != ${WPE_PKG_ENV} pkg-config --cflags --libs wpe-web-process-extension-2.0 || echo WPE-WEBKIT-MISSING
.endif

${EXTENSION}: ${.CURDIR}/WebScriptingExtension.c
	mkdir -p ${.TARGET:H}
	${CC} -bundle -O2 -Wall -Wno-nullability-completeness -mmacos-version-min=${MACOS_VERSION_MIN} \
	    --sysroot=${MINKAR_SDKROOT} ${LDFLAGS:M--ld-path=*} -o ${.TARGET} ${.ALLSRC} ${EXTENSION_FLAGS}

fmwk-install-hook: ${EXTENSION}
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
