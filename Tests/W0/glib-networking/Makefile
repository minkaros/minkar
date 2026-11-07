# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# glib-networking's own tests (W0): its installed tests, each described by
# a .test file (the GNOME convention), run by run.sh. They use the
# system's TLS module (OpenSSL), in GIO's module directory.

SUBDIR =
EXTLIB =	glib-networking
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/glib-networking
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_PATCHES = 0001-security-framework-optional.patch
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dgnutls=disabled -Dopenssl=enabled -Dlibproxy=disabled \
		-Dgnome_proxy=disabled -Denvironment_proxy=enabled \
		-Dtests=true -Dinstalled_tests=true
EXTLIB_TEST_FILES = usr/libexec/installed-tests/glib-networking \
		usr/share/installed-tests/glib-networking

.include <minkar.extlib.mk>
