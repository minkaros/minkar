# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate BC (Docs/ROADMAP.md, lane BC): the designated apps' published
# binaries, run in the image. Their probe and runner go to
# /usr/local/tests/bc; the apps themselves are downloaded on the build
# host and copied into the running VM, never into the image. Not part of
# "world": 'bmake bc-tools' before 'bmake image', then 'bmake bc-check'.

SUBDIR =	probe

.include <bsd.subdir.mk>
