# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libgpg-error, Libgcrypt's error codes (WebKit W0). The host is named
# x86_64-apple-darwin, without the version, so its lock object header
# (syscfg/lock-obj-pub.x86_64-apple-darwin.h) is found.

SUBDIR =
EXTLIB =	libgpg-error
EXTLIB_SYSTEM =	autotools
EXTLIB_OPTIONS = --disable-doc --disable-tests --disable-nls --disable-languages \
		--host=x86_64-apple-darwin \
		--enable-install-gpg-error-config
EXTLIB_LIBS =	libgpg-error.0.dylib
EXTLIB_SDK_FILES = usr/bin/gpgrt-config
EXTLIB_LICENSES = COPYING.LIB

.include <minkar.extlib.mk>
