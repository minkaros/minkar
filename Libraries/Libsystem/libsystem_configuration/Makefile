# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# libsystem_configuration: the client side of configd's DNS configuration
# (dnsinfo), network information (nwi) and configuration agents, as in
# macOS's libSystem. The sources live with configd (Apple's, APSL).

SUBDIR =
MK_WERROR = no
LIB = system_configuration
CONFIGD = ${ROOT_SOURCE_DIR}/BSD/usr.sbin/configd
SLF = /System/Library/Frameworks

.PATH: ${CONFIGD}/dnsinfo ${CONFIGD}/nwi ${CONFIGD}/libSystemConfiguration \
	${CONFIGD}/config-agent-info
SRCS = dnsinfo_copy.c network_information.c libSystemConfiguration_client.c \
	config_agent_info.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/include \
	-DPRIVATE -fblocks -Wno-nullability-completeness \
	-I${CONFIGD}/dnsinfo -I${CONFIGD}/nwi -I${CONFIGD}/libSystemConfiguration \
	-I${CONFIGD}/config-agent-info -I${CONFIGD}/Plugins/IPMonitor \
	-I${ROOT_SOURCE_DIR}/Frameworks/SystemConfiguration/compat \
	-I${.CURDIR}/../private -I${.CURDIR}/../libsystem_c \
	-I${.CURDIR}/../libdispatch/private -I${.CURDIR}/../liblaunch \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers

LDFLAGS = -Wl,-syslibroot,${MINKAR_SDKROOT}/usr/lib -umbrella System \
	-L${MINKAR_SDKROOT}/usr/lib -L${MINKAR_SDKROOT}/usr/lib/system \
	-Wl,-current_version,1061.0.2 -Wl,-compatibility_version,1 \
	-Wl,-install_name,/usr/lib/system/libsystem_configuration.dylib \
	-mmacos-version-min=${MACOS_VERSION_MIN} -lxpc -lsystem_blocks \
	-lsystem_c -lsystem_kernel -lsystem_platform -lsystem_malloc \
	-lsystem_notify -lsystem_trace -ldispatch -ldyld

# Private headers, for configd and mDNSResponder.
INCS = ${CONFIGD}/dnsinfo/dnsinfo.h ${CONFIGD}/nwi/network_information.h \
	${CONFIGD}/config-agent-info/config_agent_info.h

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib/system ${SYSROOT_DIR}/usr/lib/system \
		${MINKAR_SDKROOT}/usr/local/include
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/system/
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/system/
	cp -f ${INCS} ${MINKAR_SDKROOT}/usr/local/include/

.include <bsd.lib.mk>
