# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Brotli, the compression libsoup reads (WebKit W0): its libraries.

SUBDIR =
EXTLIB =	brotli
EXTLIB_SYSTEM =	cmake
EXTLIB_OPTIONS = -DBROTLI_DISABLE_TESTS=ON
EXTLIB_LIBS =	libbrotlicommon.1.dylib libbrotlidec.1.dylib libbrotlienc.1.dylib
EXTLIB_STAGE_REMOVE = bin/brotli
EXTLIB_LICENSES = LICENSE

.include <minkar.extlib.mk>
