# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# LaunchServices.framework, inside CoreServices.framework as on the
# reference platform (CoreServices re-exports it). For now only the
# session: the client of launchservicesd (WindowServer plan, WS4a). The
# database and the open code in this directory (LaunchServices.mm,
# LSAppRecord.mm, UTTypes.mm, LSIPC.mm) are built with Phase 3, item 4.

FRAMEWORK = LaunchServices
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_INSTALL_DIR = /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks
# Only CoreServices links it; programs get it through CoreServices.
LDFLAGS += -Wl,-umbrella,CoreServices
MK_WERROR = no

SRCS = LSSession.c LSSessionUser.c
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
	rm -rf ${dir}${FMWK_INSTALL_DIR}/${FRAMEWORK}.framework
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${dir}${FMWK_INSTALL_DIR}
	ln -sfn Versions/Current/Frameworks ${dir}${PARENT}/Frameworks
.endfor

.include <minkar.framework.mk>
