# OpenDirectory: the Objective-C API (APSL, OpenDirectory-146) on
# CFOpenDirectory (Core/), which it re-exports.
SUBDIR =
FRAMEWORK = OpenDirectory
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_BUNDLE_ID = com.apple.OpenDirectory
MK_WERROR = no

.PATH: ${.CURDIR}/Framework
SRCS =	ODContext.m ODNode.m ODQuery.m ODRecord.m ODSession.m

HEADERS = Framework/OpenDirectory.h Framework/NSOpenDirectory.h \
	Framework/ODContext.h Framework/ODNode.h Framework/ODQuery.h \
	Framework/ODRecord.h Framework/ODSession.h
PRIVATE_HEADERS = Framework/OpenDirectoryPriv.h Framework/NSOpenDirectoryPriv.h

INFO_PLIST = Framework/Info.plist

CFOD_DYLIB = ${MINKAR_SDKROOT}/System/Library/Frameworks/OpenDirectory.framework/Versions/A/Frameworks/CFOpenDirectory.framework/Versions/A/CFOpenDirectory

# Foundation/NSPrivateDecls.h from Framework/Private; <OpenDirectory/...>
# and <CFOpenDirectory/...> from the sources.
CFLAGS += -fblocks -Wno-deprecated-declarations \
	-I${.CURDIR}/Framework/Private -I${.OBJDIR}/include \
	-I${.CURDIR}/Core -I${.CURDIR} -I${.CURDIR}/DirectoryService
LDFLAGS += -framework Foundation -framework CoreFoundation \
	-Wl,-reexport_library,${CFOD_DYLIB} -lobjc

_INCLUDE_LINKS != mkdir -p ${.OBJDIR}/include && \
	ln -sfn ${.CURDIR}/Framework ${.OBJDIR}/include/OpenDirectory && \
	ln -sfn ${.CURDIR}/Core ${.OBJDIR}/include/CFOpenDirectory; echo

fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}/System/Library/Frameworks
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${dir}/System/Library/Frameworks
.endfor

.include <minkar.framework.mk>
