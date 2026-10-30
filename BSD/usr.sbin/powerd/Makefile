# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# powerd: ours, the power management daemon IOKit's power functions talk
# to (the powermanagement MIG subsystem, served from IOKit's .defs), in
# /usr/libexec, started by launchd (org.minkar.powerd).

SUBDIR =
MK_WERROR = no
WARNS ?= 2
PROG =	powerd
SRCS =	powerd.c pmServer.c
MAN =
IOKIT_HDRS = ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Versions/A
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.OBJDIR} \
	-I${IOKIT_HDRS}/PrivateHeaders -I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit -lbsm
CLEANFILES += pmServer.c pmServer.h

# The server side of IOKit's powermanagement subsystem.
pmServer.c pmServer.h: ${ROOT_SOURCE_DIR}/Kernel/IOKitUser/pwr_mgt.subproj/powermanagement.defs
	${MIG} -cc ${CC} -novouchers -DIOKIT -DPRIVATE \
	    -I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk -I${IOKIT_HDRS}/Headers \
	    -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -header /dev/null -user /dev/null -server pmServer.c \
	    -sheader pmServer.h ${.ALLSRC}

powerd.o powerd.pieo: pmServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/usr/share/man/man8 \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/powerd
	install -m 0444 ${.CURDIR}/powerd.8 ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0644 ${.CURDIR}/org.minkar.powerd.plist \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons/

.include <bsd.prog.mk>
