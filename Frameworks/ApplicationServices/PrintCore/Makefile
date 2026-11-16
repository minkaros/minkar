# PrintCore: print sessions, settings, page formats, printers (CUPS) and
# printed documents (PDF through Core Graphics). A framework inside
# ApplicationServices, which re-exports it.
FRAMEWORK = PrintCore
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_INSTALL_DIR = /System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks
MK_WERROR = no

SRCS = PrintCore.c
HEADERS = PrintCore.h PMCore.h PMDefinitions.h PMPrintSettingsKeys.h
RESOURCES = Info.plist

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
# <PrintCore/...> resolves to these sources while the framework is built.
INCLUDE_ROOT != mkdir -p ${.OBJDIR}/include-root && \
	ln -sfn ${.CURDIR} ${.OBJDIR}/include-root/PrintCore && \
	echo ${.OBJDIR}/include-root
CFLAGS += -I${INCLUDE_ROOT} -F${FMWKS} -std=gnu17
LDFLAGS += -Wl,-umbrella,ApplicationServices -F${FMWKS} -framework CoreFoundation -framework CoreGraphics \
	-framework CoreServices -lcups

PARENT = /System/Library/Frameworks/ApplicationServices.framework
fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}${FMWK_INSTALL_DIR}
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${dir}${FMWK_INSTALL_DIR}
	ln -sfn Versions/Current/Frameworks ${dir}${PARENT}/Frameworks
.endfor

.include <minkar.framework.mk>
