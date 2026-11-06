# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# GLib, GObject and GIO, under libsoup and WebKit's WPE port (WebKit W0),
# as dynamic libraries (LGPL). On pcre2, libffi, zlib and the system's
# iconv; translations are off (-Dnls=disabled), and GLib's proxy-libintl
# (LGPL) is linked into libglib rather than installed as a libintl. The
# build tools (glib-compile-resources, glib-mkenums...) are for Minkar;
# the libraries built on GLib will need them for the build host too.

SUBDIR =
EXTLIB =	glib
EXTLIB_SYSTEM =	meson
EXTLIB_PATCHES = 0001-gio-link-coreservices.patch
EXTLIB_SUBPROJECTS = proxy-libintl
EXTLIB_OPTIONS = -Dnls=disabled -Dselinux=disabled -Dxattr=false -Dlibmount=disabled \
		-Dintrospection=disabled -Dtests=false -Dman-pages=disabled \
		-Ddocumentation=false -Ddtrace=disabled -Dsystemtap=disabled \
		-Dsysprof=disabled -Dglib_debug=disabled -Dglib_assert=true \
		-Doss_fuzz=disabled -Dbsymbolic_functions=false \
		--force-fallback-for=intl -Dproxy-libintl:default_library=static
EXTLIB_STAGE_REMOVE = include/libintl.h lib/libintl.a lib/pkgconfig/intl.pc
EXTLIB_LIBS =	libglib-2.0.0.dylib libgobject-2.0.0.dylib libgio-2.0.0.dylib \
		libgmodule-2.0.0.dylib libgthread-2.0.0.dylib libgirepository-2.0.0.dylib
EXTLIB_LICENSES = COPYING LICENSES/LGPL-2.1-or-later.txt

.include <minkar.extlib.mk>
