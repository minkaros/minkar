MK_WERROR = no
FRAMEWORK = CoreServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CoreServices.c MacMemory.c Files.c Resources.c Gestalt.c IconsCore.c MDItem.c LSItemInfo.c \
	UnicodeUtilities.c
HEADERS = CoreServices.h Metadata.h MacErrors.h MacMemory.h Files.h Resources.h Gestalt.h IconsCore.h LSItemInfo.h \
	UnicodeUtilities.h

RESOURCES=Info.plist English.lproj

CFLAGS = -I${.CURDIR:H} -F${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Frameworks
SUBDIR =
# LaunchServices, FSEvents and AE (built before this one, in Frameworks/Makefile)
# live in this framework's Frameworks directory; CoreServices re-exports them.
LS_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/LaunchServices
FSE_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/FSEvents.framework/Versions/A/FSEvents
AE_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/AE.framework/Versions/A/AE
LDFLAGS += -Wl,-reexport_library,${LS_DYLIB} -Wl,-reexport_library,${FSE_DYLIB} -Wl,-reexport_library,${AE_DYLIB}
LDFLAGS += -framework CoreFoundation
# Unicode collation (UnicodeUtilities.c) on the system's ICU, whose names
# carry no version suffix.
CFLAGS.UnicodeUtilities.c = -DU_DISABLE_RENAMING=1
LDFLAGS += -licucore

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
.include <bsd.subdir.mk>
