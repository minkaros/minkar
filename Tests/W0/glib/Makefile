# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# GLib's own tests (W0): its installed tests, each described by a .test
# file (the GNOME convention), run by run.sh.

SUBDIR =
EXTLIB =	glib
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/glib
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_PATCHES = 0001-gio-link-coreservices.patch
EXTLIB_SUBPROJECTS = proxy-libintl
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dnls=disabled -Dselinux=disabled -Dxattr=false -Dlibmount=disabled \
		-Dintrospection=disabled -Dtests=true -Dinstalled_tests=true -Dman-pages=disabled \
		-Ddocumentation=false -Ddtrace=disabled -Dsystemtap=disabled \
		-Dsysprof=disabled -Dglib_debug=disabled -Dglib_assert=true \
		-Doss_fuzz=disabled -Dbsymbolic_functions=false \
		--force-fallback-for=intl -Dproxy-libintl:default_library=static \
		-Dgio_module_dir=/usr/lib/webkit/gio/modules
EXTLIB_TEST_FILES = usr/libexec/installed-tests/glib usr/share/installed-tests/glib

.include <minkar.extlib.mk>
