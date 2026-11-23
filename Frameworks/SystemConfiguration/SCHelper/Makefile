# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SCHelper: SystemConfiguration's privileged helper (APSL, ../helper),
# unchanged. SCPreferences opened with an authorization by a process that
# is not root (the Network pane behind its lock, networksetup) reads,
# locks, commits and applies the system's network preferences through it;
# it checks the authorization with authd (system.services.systemconfiguration.network).
# Inside the framework's Helpers, started by launchd on demand
# (org.minkar.SCHelper; the maintainer's choice, 2026-11-23: authorization in
# full).

SUBDIR =
MK_WERROR = no
WARNS ?= 0
SC =	${.CURDIR}/..
CONFIGD = ${ROOT_SOURCE_DIR}/BSD/usr.sbin/configd
SLF =	/System/Library/Frameworks
HELPERS = ${SLF}/SystemConfiguration.framework/Versions/A/Helpers
.PATH: ${SC}/helper ${.OBJDIR}
PROG =	SCHelper
SRCS =	SCHelper_server.c helperServer.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-everything \
	-I${.OBJDIR}/include -I${.OBJDIR} -I${SC} -I${SC}/helper \
	-I${CONFIGD}/dnsinfo -I${CONFIGD}/nwi -I${CONFIGD}/libSystemConfiguration \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${CONFIGD}/Plugins/common -I${CONFIGD}/IPMonitorControl \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers \
	-idirafter ${SC}/compat
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework SystemConfiguration -framework Security -lbsm
MIGFLAGS = -novouchers --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -DPRIVATE -I${SC}

# <SystemConfiguration/...> includes find the sources.
${.OBJDIR}/include/SystemConfiguration:
	mkdir -p ${.OBJDIR}/include
	ln -sfn ${SC} ${.TARGET}

helperServer.c helperServer.h: ${SC}/helper/helper.defs ${.OBJDIR}/include/SystemConfiguration
	${MIG} ${MIGFLAGS} -I${SC}/helper -user /dev/null -server helperServer.c \
		-header /dev/null -sheader helperServer.h ${SC}/helper/helper.defs

${OBJS}: helperServer.h ${.OBJDIR}/include/SystemConfiguration

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${HELPERS} ${SYSROOT_DIR}/System/Library/LaunchDaemons
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}${HELPERS}/${PROG}
	install -C -m 0644 ${.CURDIR}/org.minkar.SCHelper.plist ${SYSROOT_DIR}/System/Library/LaunchDaemons/

.include <bsd.prog.mk>
