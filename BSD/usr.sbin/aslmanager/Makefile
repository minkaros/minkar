# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# aslmanager, Apple's ASL store manager (syslog project, APSL, in the
# tree; approved 2026-10-24): rotates and expires /var/log/asl and the
# files asl.conf names. launchd runs it (com.apple.aslmanager.plist).

SUBDIR =
MK_WERROR = no
PROG =	aslmanager
MAN =
BINDIR = /usr/sbin

ASLCOMMON = ${ROOT_SOURCE_DIR}/BSD/syslog/aslcommon
LIBASL =    ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_asl
SLF =	/System/Library/Frameworks

.PATH: ${.OBJDIR} ${ASLCOMMON}
SRCS =	aslmanager.c daemon.c asl_common.c asl_ipcUser.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-nullability-completeness \
	-Wno-deprecated-declarations \
	-I${.OBJDIR} -I${.CURDIR} -I${ASLCOMMON} -I${LIBASL} \
	-I${ROOT_SOURCE_DIR}/Libraries/zlib \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD =	-lbsm ${MINKAR_SDKROOT}/usr/lib/libz.a

MIGFLAGS = -novouchers --sysroot=${MINKAR_SDKROOT} \
	-mmacos-version-min=${MACOS_VERSION_MIN} -DPRIVATE

asl_ipcUser.c asl_ipc.h: ${ASLCOMMON}/asl_ipc.defs
	${MIG} ${MIGFLAGS} -user asl_ipcUser.c -header asl_ipc.h \
		-server /dev/null -sheader /dev/null ${ASLCOMMON}/asl_ipc.defs

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR}
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/

.include <bsd.prog.mk>

${OBJS}: asl_ipc.h
