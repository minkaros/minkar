# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libsystem_sandbox: sandbox_init and sandbox_check (sandbox.h). Profiles
# are compiled here (SBPL, sbpl.c) and enforced by the kernel's sandbox
# policy, which evaluates them with the same sbeval.c and sbregex.c. Our
# system.sb is the base profile daemons' profiles import.

SUBDIR =
MK_WERROR = no
LIB = system_sandbox
SRCS = sandbox.c sbpl.c sbeval.c sbregex.c

CFLAGS = -mmacos-version-min=${MACOS_VERSION_MIN} --sysroot=${MINKAR_SDKROOT} \
	-Wno-nullability-completeness -I${.CURDIR}

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/system/libsystem_sandbox.dylib \
	-Wl,-current_version,1217.0.27 -Wl,-compatibility_version,1 \
	-L${MINKAR_SDKROOT}/usr/lib/system -L${MINKAR_SDKROOT}/usr/lib \
	-umbrella System -lsystem_c -lsystem_kernel -lsystem_platform -lsystem_malloc -ldyld

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
	  -Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib/system ${SYSROOT_DIR}/usr/lib/system
	rsync -cL ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/system
	rsync -cL ${.TARGET} ${SYSROOT_DIR}/usr/lib/system
	mkdir -p ${MINKAR_SDKROOT}/usr/include ${MINKAR_SDKROOT}/usr/local/include/minkar
	rsync -c ${.CURDIR}/sandbox.h ${MINKAR_SDKROOT}/usr/include/sandbox.h
	# The compiled form and its evaluator, for the kernel's policy.
	rsync -c ${.CURDIR}/sbblob.h ${MINKAR_SDKROOT}/usr/local/include/minkar/sbblob.h
	# The base profile system daemons' profiles import (import "system.sb").
	mkdir -p ${SYSROOT_DIR}/System/Library/Sandbox/Profiles
	rsync -c ${.CURDIR}/system.sb ${SYSROOT_DIR}/System/Library/Sandbox/Profiles/system.sb

lib${LIB}.dylib: ${.OBJDIR}/lib${LIB}.dylib

# Optimised: it was built without any -O.
CFLAGS += -Os
.include <bsd.lib.mk>
