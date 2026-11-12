
BUILD_STAMP != cd ${.CURDIR} && git log -1|head -1|cut -c8-14|tr '[a-z]' '[A-Z]'

#SUBDIR= Desktop_Pictures
SUBDIR= Colors LaunchDaemons Fonts Icons UserTemplate ../Kernel/Extensions/MinkarDRM/firmware
SUBDIR_PARALLEL=

all: SystemVersion.plist

SystemVersion.plist: ${.CURDIR}/SystemVersion.plist.in ${.CURDIR:H}/Makefile .EXEC
	sed -e 's/@BUILD_STAMP@/${BUILD_STAMP}/' \
	  -e 's/@PROD_FAMILY@/${PROD_FAMILY}/' \
	  -e 's/@PROD_VERSION@/${PROD_VERSION}/' \
	  -e 's/@PROD_COMPAT_VERSION@/${PROD_COMPAT_VERSION}/' \
	  < ${.CURDIR}/SystemVersion.plist.in > ${.OBJDIR}/${.TARGET}
	mkdir -p ${SYSROOT_DIR}/System/Library/CoreServices
	rsync -cL ${.OBJDIR}/${.TARGET} ${SYSROOT_DIR}/System/Library/CoreServices

.include <bsd.subdir.mk>
