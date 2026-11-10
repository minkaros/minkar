# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# WPE WebKit (WebKit W2), from WebKit's sources at WPE WebKit 2.54.2's
# commit (MANIFEST): WebCore, the web content and network processes and
# the WPE platform's headless display, in /usr/lib/webkit (private: W3
# builds WebKit.framework on it). Its processes talk over Unix sockets, not
# Mach and XPC; it draws with Skia on Mesa's EGL. Media (GStreamer) is left
# out until the system's own media stack (AU6). The patches make it build
# for API 19 (10.15) on Minkar's toolchain, on top of JavaScriptCore's.

SUBDIR =
EXTLIB =	wpewebkit
EXTLIB_SYSTEM =	cmake
JSC_PATCHES =	0001-libpas-unfair-lock-flags-where-public.patch \
		0002-wtf-exception-port-option-where-the-kernel-has-it.patch \
		0003-jsconly-darwin-framework-search-path.patch \
		0004-wtf-packed-includes-mach-vm-types.patch \
		0005-wtf-jsconly-darwin-time-zone-notifications.patch \
		0006-cmake-codesign-executable.patch
EXTLIB_PATCHES = ${JSC_PATCHES:@p@../../javascriptcore/patches/${p}@} \
		0001-cmake-wpe-on-darwin-without-cocoa-bundles.patch \
		0002-cmake-glib-include-dir-in-sysroot.patch \
		0003-wtf-wpe-on-darwin.patch \
		0004-webcore-crypto-wpe-on-darwin.patch \
		0005-webkit-ipc-unix-sockets-on-darwin.patch \
		0006-webkit-c-api-headers-wpe-on-darwin.patch \
		0007-egl-surfaceless-display-without-native-type.patch \
		0008-wpe-backing-store-without-libdrm.patch \
		0009-skia-sources-the-library-uses.patch \
		0010-webkit-ipc-glib-stream-sockets-on-darwin.patch \
		0011-webkit-glib-content-types-as-mime-types.patch
# Libraries of WebKit's own (BSD/lib, private) are in the SDK's
# usr/local/webkit and /usr/lib/webkit.
EXTLIB_OPTIONS = -DPORT=WPE -DDEVELOPER_MODE=OFF \
		-DCMAKE_INCLUDE_PATH=${PRIVATE_DEV}/include -DCMAKE_LIBRARY_PATH=${PRIVATE_LIB} \
		-DCMAKE_INSTALL_LIBEXECDIR=lib/webkit \
		-DENABLE_DOCUMENTATION=OFF -DENABLE_INTROSPECTION=OFF -DENABLE_JOURNALD_LOG=OFF \
		-DENABLE_WPE_PLATFORM=ON -DENABLE_WPE_PLATFORM_HEADLESS=ON \
		-DENABLE_WPE_PLATFORM_DRM=OFF -DENABLE_WPE_PLATFORM_WAYLAND=OFF \
		-DENABLE_WPE_LEGACY_API=OFF -DENABLE_WPE_QT_API=OFF \
		-DUSE_ATK=OFF -DUSE_FLITE=OFF -DUSE_GBM=OFF -DUSE_LIBDRM=OFF \
		-DUSE_LIBBACKTRACE=OFF -DUSE_LIBHYPHEN=OFF -DUSE_AVIF=OFF -DUSE_LCMS=OFF \
		-DUSE_JPEGXL=OFF -DUSE_WOFF2=ON -DUSE_GSTREAMER=OFF \
		-DENABLE_VIDEO=OFF -DENABLE_WEB_AUDIO=OFF -DENABLE_MEDIA_STREAM=OFF \
		-DENABLE_MEDIA_RECORDER=OFF -DENABLE_WEB_CODECS=OFF -DENABLE_ENCRYPTED_MEDIA=OFF \
		-DENABLE_SPEECH_SYNTHESIS=OFF -DENABLE_GAMEPAD=OFF -DENABLE_WEBDRIVER=OFF \
		-DENABLE_PDFJS=OFF -DENABLE_SPELLCHECK=OFF -DENABLE_BUBBLEWRAP_SANDBOX=OFF \
		-DUSE_SYSPROF_CAPTURE=OFF -DENABLE_WEBGL=OFF -DENABLE_GPU_PROCESS=OFF \
		-DENABLE_API_TESTS=OFF -DENABLE_MINIBROWSER=OFF \
		-DUSE_SYSTEM_MALLOC=OFF -DUSE_SYSTEM_UNIFDEF=ON -DUNIFDEF_EXECUTABLE=/usr/bin/unifdef \
		-DMig_EXECUTABLE=${TC}/mig -DWEBKIT_CODESIGN_EXECUTABLE=${TC}/codesign
# One library (the WPE platform is in it); its processes in /usr/lib/webkit,
# and the injected bundle the web process loads.
EXTLIB_LIBS =	libWPEWebKit-2.0.1.dylib
EXTLIB_MODULES = lib/webkit/wpe-webkit-2.0/WPEWebProcess \
		lib/webkit/wpe-webkit-2.0/WPENetworkProcess \
		lib/wpe-webkit-2.0/injected-bundle/libWPEInjectedBundle.so
EXTLIB_LICENSES = Source/WebCore/LICENSE-LGPL-2.1 Source/WebCore/LICENSE-APPLE

.include <minkar.extlib.mk>
