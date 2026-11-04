MK_WERROR = no
FRAMEWORK = CoreServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CoreServices.c
HEADERS = CoreServices.h 

RESOURCES=Info.plist English.lproj

CFLAGS = -I${.CURDIR:H} -F${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Frameworks
SUBDIR =
# LaunchServices and FSEvents (built before this one, in Frameworks/Makefile)
# live in this framework's Frameworks directory; CoreServices re-exports them.
LS_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/LaunchServices
FSE_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/FSEvents.framework/Versions/A/FSEvents
LDFLAGS += -Wl,-reexport_library,${LS_DYLIB} -Wl,-reexport_library,${FSE_DYLIB}

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
.include <bsd.subdir.mk>
