# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libpsl, the Public Suffix List for libsoup's cookies (WebKit W0), with
# the list its release carries built in and no IDNA library.

SUBDIR =
EXTLIB =	libpsl
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Druntime=no -Dbuiltin=true -Dtests=false -Ddocs=false
EXTLIB_LIBS =	libpsl.5.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
