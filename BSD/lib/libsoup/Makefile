# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libsoup 3, WebKit's HTTP library (WebKit W0), on GLib, nghttp2, Brotli,
# libpsl and SQLite; TLS through GIO (glib-networking).

SUBDIR =
EXTLIB =	libsoup
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dgssapi=disabled -Dntlm=disabled -Dbrotli=enabled -Dtls_check=false \
		-Dintrospection=disabled -Dvapi=disabled -Ddocs=disabled -Dtests=false \
		-Dsysprof=disabled -Dpkcs11_tests=disabled -Dautobahn=disabled
EXTLIB_LIBS =	libsoup-3.0.0.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
