# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# HarfBuzz, text shaping for WebKit (W0), on FreeType, GLib and ICU
# (libicucore, through the SDK's usr/local headers).

SUBDIR =
EXTLIB =	harfbuzz
EXTLIB_SYSTEM =	meson
EXTLIB_OPTIONS = -Dfreetype=enabled -Dglib=enabled -Dicu=enabled -Dgobject=disabled -Dcairo=disabled \
		-Dcoretext=disabled -Dchafa=disabled -Dgraphite2=disabled -Dintrospection=disabled \
		-Ddocs=disabled -Dtests=disabled -Dbenchmark=disabled -Dutilities=disabled
EXTLIB_LIBS =	libharfbuzz.0.dylib libharfbuzz-icu.0.dylib libharfbuzz-subset.0.dylib
EXTLIB_LICENSES = COPYING

.include <minkar.extlib.mk>
