# CFOpenDirectory: OpenDirectory's C API (APSL, OpenDirectory-146), a
# framework inside OpenDirectory.framework as on the reference platform.
# It talks to Minkar's opendirectoryd (CoreServices/opendirectoryd) through
# the protocol in opendirectory/ (Minkar's own).
SUBDIR =
FRAMEWORK = CFOpenDirectory
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_BUNDLE_ID = com.apple.CFOpenDirectory
FMWK_INSTALL_DIR = /System/Library/Frameworks/OpenDirectory.framework/Versions/A/Frameworks
MK_WERROR = no

SRCS =	CFODContext.c CFODNode.c CFODQuery.c CFODRecord.c CFODSession.c \
	CFODTrigger.c constants.c errors.c extauth.c format.c odxpc.c rb.c \
	schema.c transaction.c

HEADERS = CFOpenDirectory.h CFOpenDirectoryConstants.h CFODContext.h \
	CFODNode.h CFODQuery.h CFODRecord.h CFODSession.h
PRIVATE_HEADERS = CFOpenDirectoryPriv.h CFOpenDirectoryConstantsPriv.h \
	CFODTrigger.h

INFO_PLIST = Info.plist

# <CFOpenDirectory/...> and <DirectoryService/...> from the sources.
CFLAGS += -fblocks -Wno-deprecated-declarations -Wno-nonnull \
	-I${.OBJDIR}/include -I${.CURDIR} -I${.CURDIR:H} \
	-I${.CURDIR:H}/DirectoryService
LDFLAGS += -Wl,-umbrella,OpenDirectory -framework CoreFoundation \
	-framework SystemConfiguration -lobjc \
	-Wl,-exported_symbols_list,${.CURDIR:H}/CFOpenDirectory.exp

_INCLUDE_LINKS != mkdir -p ${.OBJDIR}/include && \
	ln -sfn ${.CURDIR} ${.OBJDIR}/include/CFOpenDirectory; echo

PARENT = /System/Library/Frameworks/OpenDirectory.framework
fmwk-install-hook:
	rsync -rlc ${.CURDIR}/English.lproj ${RESOURCES_DIR}/
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}${FMWK_INSTALL_DIR}
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${dir}${FMWK_INSTALL_DIR}
	ln -sfn Versions/Current/Frameworks ${dir}${PARENT}/Frameworks
.endfor

.include <minkar.framework.mk>
