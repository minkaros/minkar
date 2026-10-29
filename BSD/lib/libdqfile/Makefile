# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libdqfile: our own reading and writing of Darwin's quota files (the
# hash table <sys/quota.h> describes) and the quotactl(2) calls that
# quota, quotacheck, quotaon, edquota and repquota share. Static,
# internal.

SUBDIR =
LIB =	dqfile
SRCS =	dqfile.c
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR}

all: lib${LIB}.a

.include <bsd.lib.mk>
