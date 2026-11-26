# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Independent programs: built in parallel.
SUBDIR_PARALLEL =
SUBDIR = dhcpclient getty kextstart atrun launchproxy ntalkd espeak-ng authorization-trampoline

# Optimised: the programs were built without any -O (the top Makefile's
# CFLAGS kept sys.mk's -O2 away).
CFLAGS += -O2
.export CFLAGS
.include <bsd.subdir.mk>
