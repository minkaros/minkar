# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# diskarbitrationd: Apple's (DiskArbitration-297.70.1, APSL), from the
# tree in Frameworks/DiskArbitration, in /usr/libexec, started by launchd
# (org.minkar.diskarbitrationd). It probes, names and mounts the disks
# IOKit publishes, through the file system bundles in
# /System/Library/Filesystems, and serves DiskArbitration.framework.
# autodiskmount.plist: external and removable disks mount without a user
# logged in at the console (macOS's AutomountDisksWithoutUserLogin), as
# there is no login window yet to tell diskarbitrationd of one.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
DA = ${ROOT_SOURCE_DIR}/Frameworks/DiskArbitration
.PATH: ${DA}/diskarbitrationd ${DA}/compat
PROG =	diskarbitrationd
SRCS =	DAAgent.c DABase.c DACallback.c DACommand.c DADialog.c DADisk.c \
	DADissenter.c DAFileSystem.c DAInternal.c DALog.c DAMain.c DAMount.c \
	DAPrivate.c DAProbe.c DAQueue.c DARequest.c DAServer.c DASession.c \
	DAStage.c DASupport.c DAThread.c fstab.c vsdb.c FSPrivate.c \
	DAServerServer.c
MAN =	diskarbitrationd.8
SLF =	/System/Library/Frameworks
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -fcommon -DPRIVATE -D__DISKARBITRATIOND__ \
	-Wno-nullability-completeness \
	-Wno-deprecated-declarations -Wno-int-conversion \
	-I${.OBJDIR} -I${DA}/diskarbitrationd -I${DA}/compat \
	-I${MINKAR_SDKROOT}${SLF}/SystemConfiguration.framework/Versions/A/PrivateHeaders \
	-I${MINKAR_SDKROOT}${SLF}/IOKit.framework/Versions/A/PrivateHeaders \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit \
	-framework SystemConfiguration -lbsm
CLEANFILES += DAServerServer.c DAServerServer.h

DAServerServer.c DAServerServer.h: ${DA}/diskarbitrationd/DAServer.defs
	${MIG} -novouchers --sysroot=${MINKAR_SDKROOT} \
	    -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -I${DA}/diskarbitrationd -I${DA}/compat \
	    -header /dev/null -user /dev/null -server DAServerServer.c \
	    -sheader DAServerServer.h ${.ALLSRC}

${SRCS:N*Server.c:R:S/$/.o/}: DAServerServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/usr/share/man/man8 \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}
	install -m 0444 ${DA}/diskarbitrationd/diskarbitrationd.8 \
	    ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0644 ${.CURDIR}/org.minkar.diskarbitrationd.plist \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons/
	mkdir -p ${SYSROOT_DIR}/Library/Preferences/SystemConfiguration
	install -m 0644 ${.CURDIR}/autodiskmount.plist \
	    ${SYSROOT_DIR}/Library/Preferences/SystemConfiguration/

.include <bsd.prog.mk>
