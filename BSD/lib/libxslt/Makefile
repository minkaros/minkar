# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libxslt and libexslt, XSLT for WebKit (W0), as /usr/lib/libxslt.1.dylib
# and libexslt.0.dylib, as the reference platform has them, on libxml2.

SUBDIR =
EXTLIB =	libxslt
EXTLIB_PUBLIC =	yes
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --without-python --without-crypto --without-debugger --without-profiler
EXTLIB_LIBS =	libxslt.1.dylib libexslt.0.dylib
EXTLIB_LICENSES = Copyright

.include <minkar.extlib.mk>
