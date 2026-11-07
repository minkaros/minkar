# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libsoup's own tests (W0): its installed tests, each described by a .test
# file (the GNOME convention), run by run.sh. The four that need an Apache
# server (auth, connection, range, proxy) use Tests/W0/httpd's, in
# /usr/local/tests/w0-tools/httpd; a test-only patch names it, as
# libsoup would look for one where it is built.

SUBDIR =
EXTLIB =	libsoup
EXTLIB_TEST =	yes
EXTLIB_DIR =	${ROOT_SOURCE_DIR}/BSD/lib/libsoup
EXTLIB_MANIFEST = ${EXTLIB_DIR}/MANIFEST
EXTLIB_PATCHES = 0001-tests-apache-of-the-target.patch \
		0002-tests-no-rfc2069-digest-with-httpd-2.4.69.patch
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dgssapi=disabled -Dntlm=disabled -Dbrotli=enabled -Dtls_check=false \
		-Dintrospection=disabled -Dvapi=disabled -Ddocs=disabled \
		-Dtests=true -Dinstalled_tests=true \
		-Dsysprof=disabled -Dpkcs11_tests=disabled -Dautobahn=disabled \
		-Dapache_httpd=/usr/local/tests/w0-tools/httpd/bin/httpd \
		-Dapache_module_dir=/usr/local/tests/w0-tools/httpd/modules
EXTLIB_TEST_FILES = usr/libexec/installed-tests/libsoup-3.0 usr/share/installed-tests/libsoup-3.0

.include <minkar.extlib.mk>
