# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# LaunchServices.framework, inside CoreServices.framework as on the
# reference platform (CoreServices re-exports it): the client of
# launchservicesd, its session (WindowServer plan, WS4a) and, since
# Phase 3 item 4, opening items and finding their applications (LSOpen.c,
# on the daemon's database).

FRAMEWORK = LaunchServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_INSTALL_DIR = /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks
# Only CoreServices links it; programs get it through CoreServices.
LDFLAGS += -Wl,-umbrella,CoreServices -framework CoreFoundation
MK_WERROR = no

SRCS = LSSession.c LSSessionUser.c LSOpen.c UTType.c UTCoreTypes.c LSConstants.c LSSharedFileList.c LSItemFSRef.c
HEADERS = LaunchServices.h LSInfo.h LSOpen.h UTType.h UTCoreTypes.h LSQuarantine.h LSSharedFileList.h
PRIVATE_HEADERS = LSSession.h
MODULEMAP = LaunchServices.modulemap
CFLAGS += -I${.OBJDIR} -I${.CURDIR}
CLEANFILES += LSSessionUser.c LSSessionUser.h

# The client side of the MIG interface.
LSSessionUser.c LSSessionUser.h: ${.CURDIR}/LSSession.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${.CURDIR} \
	    -user LSSessionUser.c -header LSSessionUser.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}

LSSession.o: LSSessionUser.h

PARENT = /System/Library/Frameworks/CoreServices.framework
fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}${FMWK_INSTALL_DIR}
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${dir}${FMWK_INSTALL_DIR}
	ln -sfn Versions/Current/Frameworks ${dir}${PARENT}/Frameworks
.endfor

.include <minkar.framework.mk>
