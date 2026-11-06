# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W0 (WebKit's dependencies): each external library of BSD/lib built
# again with its own tests, installed in /usr/local/tests/w0 ('bmake
# w0-tests', then 'image'); 'bmake w0-check' boots the image and runs them
# (runner/run-w0.sh). The tests are the projects' own; what they need and
# Minkar does not have (a network, a D-Bus session) is left out in each
# run.sh, with the reason.

SUBDIR =	runner pcre2 expat glib

.include <bsd.subdir.mk>
