#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pkg-config-none.sh: a pkg-config for cross-building ports whose
# configure insists on one. It knows no packages, so every library comes
# from the *_CFLAGS and *_LIBS a port's Makefile gives, never from the
# build machine. It only says what version it is.

case "$1" in
--version)		echo 0.29.2 ;;
--atleast-pkgconfig-version) exit 0 ;;
*)			exit 1 ;;
esac
