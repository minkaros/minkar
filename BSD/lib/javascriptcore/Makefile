# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# JavaScriptCore (WebKit W1), from WebKit's sources at WPE WebKit 2.54.2's
# commit (MANIFEST): WebKit's JSCOnly port, a framework with its JIT, in
# /usr/lib/webkit (private until W3 brings the public framework's
# Objective-C API), with the jsc shell in its Helpers. ICU is the
# system's (libicucore); WTF uses Mach exception ports (mig). The patches
# make it build for API 19 (10.15) on Minkar's toolchain.

SUBDIR =
EXTLIB =	javascriptcore
EXTLIB_SYSTEM =	cmake
EXTLIB_PATCHES = 0001-libpas-unfair-lock-flags-where-public.patch \
		0002-wtf-exception-port-option-where-the-kernel-has-it.patch \
		0003-jsconly-darwin-framework-search-path.patch \
		0004-wtf-packed-includes-mach-vm-types.patch \
		0005-wtf-jsconly-darwin-time-zone-notifications.patch \
		0006-cmake-codesign-executable.patch
EXTLIB_OPTIONS = -DPORT=JSCOnly -DENABLE_STATIC_JSC=OFF -DDEVELOPER_MODE=OFF \
		-DENABLE_API_TESTS=OFF -DENABLE_TOOLS=OFF -DUSE_SYSTEM_MALLOC=OFF \
		-DUSE_HEADER_MAPS=OFF -DMig_EXECUTABLE=${TC}/mig \
		-DWEBKIT_CODESIGN_EXECUTABLE=${TC}/codesign
EXTLIB_FRAMEWORKS = JavaScriptCore
EXTLIB_HELPERS = bin/jsc:JavaScriptCore
EXTLIB_LICENSES = Source/JavaScriptCore/COPYING.LIB

.include <minkar.extlib.mk>
