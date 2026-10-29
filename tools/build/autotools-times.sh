#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# autotools-times.sh DIR: give the files of an upstream release unpacked
# in DIR the times autotools gave them, so that make does not try to
# rebuild aclocal.m4, configure or the Makefile.in files (with aclocal,
# autoconf or automake, which the build machine may not have). A git
# checkout gives every file the time it was written, in no set order.
#
# Every file gets one time, then the generated ones later, in the order
# they are made: aclocal.m4; configure, config.h.in and the stamps;
# Makefile.in. Only times change, never contents.

set -e
dir=${1:?usage: autotools-times.sh DIR}
t=$(date +%s)
t=$((t - 10))

find "$dir" -type f -exec touch -h -d "@$t" {} +
find "$dir" -type f -name aclocal.m4 -exec touch -d "@$((t + 1))" {} +
find "$dir" -type f \( -name configure -o -name config.h.in -o \
    -name 'stamp-h*' -o -name configure.sh \) -exec touch -d "@$((t + 2))" {} +
find "$dir" -type f -name Makefile.in -exec touch -d "@$((t + 3))" {} +
