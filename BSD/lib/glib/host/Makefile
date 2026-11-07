# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# GLib for the build host: the tools the libraries built on GLib run while
# they build (glib-mkenums, glib-genmarshal, glib-compile-resources,
# glib-compile-schemas, gdbus-codegen), from the same pinned release.

SUBDIR =
EXTLIB =	glib
EXTLIB_HOST =	yes
EXTLIB_MANIFEST = ${.CURDIR}/../MANIFEST
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dnls=disabled -Dselinux=disabled -Dxattr=false -Dlibmount=disabled \
		-Dintrospection=disabled -Dtests=false -Dman-pages=disabled \
		-Ddocumentation=false -Ddtrace=disabled -Dsystemtap=disabled \
		-Dsysprof=disabled -Dglib_debug=disabled

.include <minkar.extlib.mk>
