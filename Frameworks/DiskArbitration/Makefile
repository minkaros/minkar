# DiskArbitration.framework and diskarbitrationd, from Apple's
# DiskArbitration-297.70.1 (APSL), the release of xnu 19.6's macOS.
#
# The tree is Apple's: DiskArbitration/ is the framework (it shares some
# files of diskarbitrationd/), diskarbitrationd/ the daemon. compat/ has
# Minkar's stand-ins (BSD-2): the Authorization types (no Security
# framework yet; diskarbitrationd decides a user's rights), the optical
# media classes. The daemon builds from BSD/usr.sbin/diskarbitrationd. __MINKAR__ marks the
# changes made for Minkar.

SUBDIR =
MK_WERROR = no
FRAMEWORK = DiskArbitration
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_CURRENT_VERSION = 297.70.1
FMWK_BUNDLE_ID = com.apple.DiskArbitration

SLF = /System/Library/Frameworks

.PATH: ${.CURDIR}/DiskArbitration ${.OBJDIR}
SRCS =	DADisk.c DADissenter.c DAInternal.c DASession.c DiskArbitration.c \
	DiskArbitrationPrivate.c DAServerUser.c

HEADERS = DiskArbitration/DiskArbitration.h DiskArbitration/DADisk.h \
	DiskArbitration/DADissenter.h DiskArbitration/DASession.h
PRIVATE_HEADERS = DiskArbitration/DiskArbitrationPrivate.h

INFO_PLIST = DiskArbitration/Info.plist
MODULEMAP = DiskArbitration.modulemap

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -fcommon -DPRIVATE -Wno-nullability-completeness \
	-Wno-deprecated-declarations -Wno-int-conversion \
	-I${.OBJDIR}/include -I${.OBJDIR} -I${.CURDIR}/DiskArbitration \
	-I${.CURDIR}/diskarbitrationd -I${.CURDIR}/compat \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-framework CoreFoundation -framework IOKit -lSystem

MIGFLAGS = -novouchers --sysroot=${MINKAR_SDKROOT} \
	-mmacos-version-min=${MACOS_VERSION_MIN} -I${.CURDIR}/diskarbitrationd \
	-I${.CURDIR}/compat

# <DiskArbitration/...> includes find the sources.
${.OBJDIR}/include/DiskArbitration:
	mkdir -p ${.OBJDIR}/include
	ln -sfn ${.CURDIR}/DiskArbitration ${.TARGET}

DAServerUser.c DAServer.h: ${.CURDIR}/diskarbitrationd/DAServer.defs
	${MIG} ${MIGFLAGS} -user DAServerUser.c -server /dev/null \
		-header DAServer.h -sheader /dev/null ${.ALLSRC}

fmwk-install-hook:
	cp -fR ${.CURDIR}/DiskArbitration/English.lproj ${RESOURCES_DIR}/
	mkdir -p ${MINKAR_SDKROOT}${SLF} ${SYSROOT_DIR}${SLF}
	cp -Rf ${FRAMEWORK_DIR} ${MINKAR_SDKROOT}${SLF}/
	cp -Rf ${FRAMEWORK_DIR} ${SYSROOT_DIR}${SLF}/

.include <minkar.framework.mk>

${OBJS} ${POBJS} ${SOBJS}: DAServer.h ${.OBJDIR}/include/DiskArbitration
