# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# glib-networking, GIO's TLS (WebKit W0): its OpenSSL backend, a module
# GIO loads from /usr/lib/gio/modules, trusting OpenSSL's store (/etc/ssl)
# while Minkar has no Security framework. Built after usr.bin, which
# builds OpenSSL (BSD/Makefile).

SUBDIR =
EXTLIB =	glib-networking
EXTLIB_SYSTEM =	meson
EXTLIB_PATCHES = 0001-security-framework-optional.patch
EXTLIB_OPTIONS = -Dgnutls=disabled -Dopenssl=enabled -Dlibproxy=disabled \
		-Dgnome_proxy=disabled -Denvironment_proxy=enabled -Dinstalled_tests=false
EXTLIB_MODULES = lib/gio/modules/libgioopenssl.so lib/gio/modules/libgioenvironmentproxy.so
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
