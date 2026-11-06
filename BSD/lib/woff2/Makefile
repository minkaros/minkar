# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# WOFF2, web fonts for WebKit (W0), on Brotli.

SUBDIR =
EXTLIB =	woff2
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DCANONICAL_PREFIXES=ON -DNOISY_LOGGING=OFF
EXTLIB_LIBS =	libwoff2common.1.0.2.dylib libwoff2dec.1.0.2.dylib libwoff2enc.1.0.2.dylib
EXTLIB_LICENSES = LICENSE

.include <minkar.extlib.mk>
