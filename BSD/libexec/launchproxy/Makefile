# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# launchproxy: launchd's helper (launchproxy.c, Apache-2.0, from Apple's
# launchd like BSD/sbin/launchd), which launchd runs for jobs with
# inetdCompatibility: it accepts the connection, or hands over the
# datagram socket, and starts the job's program on it, as inetd did.
# Installed as /usr/libexec/launchproxy, where launchd looks for it.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	launchproxy
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/launchproxy

.include <bsd.prog.mk>
