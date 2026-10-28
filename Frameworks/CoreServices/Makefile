MK_WERROR = no
FRAMEWORK = CoreServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CoreServices.c
HEADERS = CoreServices.h 

RESOURCES=Info.plist English.lproj

CFLAGS = -I${.CURDIR:H}
SUBDIR =

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

#SUBDIR = LaunchServices

.include <minkar.framework.mk>
.include <bsd.subdir.mk>
