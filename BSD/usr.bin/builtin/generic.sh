#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# /usr/bin/cd, /usr/bin/umask and the others: POSIX asks for the regular
# built-in utilities to be files too, so exec, find -exec, xargs and env
# can run them. Each name is this script; it runs sh's built-in of the
# name it was started by (lowercased: the root volume ignores case).
builtin "$(printf '%s\n' "${0##*/}" | tr '[:upper:]' '[:lower:]')" ${1+"$@"}
