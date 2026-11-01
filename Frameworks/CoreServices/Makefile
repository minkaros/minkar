MK_WERROR = no
FRAMEWORK = CoreServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CoreServices.c
HEADERS = CoreServices.h 

RESOURCES=Info.plist English.lproj

CFLAGS = -I${.CURDIR:H}
SUBDIR =
# LaunchServices (built before this one, in Frameworks/Makefile) lives in
# this framework's Frameworks directory, and CoreServices re-exports it.
LS_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/LaunchServices
LDFLAGS += -Wl,-reexport_library,${LS_DYLIB}

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
.include <bsd.subdir.mk>
