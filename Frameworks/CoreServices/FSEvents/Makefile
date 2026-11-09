# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# FSEvents.framework (Atlas.md, K0), inside CoreServices.framework as
# on the reference platform (CoreServices re-exports it): the client of
# fseventsd (CoreServices/fseventsd), through fseventsd.defs.

SUBDIR =
FRAMEWORK = FSEvents
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_INSTALL_DIR = /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks
# Only CoreServices links it; programs get it through CoreServices.
LDFLAGS += -Wl,-umbrella,CoreServices -framework CoreFoundation
MK_WERROR = no

SRCS = FSEvents.c fseventsdUser.c
HEADERS = FSEvents.h
CFLAGS += -I${.OBJDIR} -I${.CURDIR}
CLEANFILES += fseventsdUser.c fseventsd.h

# The client side of the MIG interface.
fseventsdUser.c fseventsd.h: ${.CURDIR}/fseventsd.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${.CURDIR} \
	    -user fseventsdUser.c -header fseventsd.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}

FSEvents.o: fseventsd.h

PARENT = /System/Library/Frameworks/CoreServices.framework
fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}${FMWK_INSTALL_DIR}
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${dir}${FMWK_INSTALL_DIR}
	ln -sfn Versions/Current/Frameworks ${dir}${PARENT}/Frameworks
.endfor

.include <minkar.framework.mk>
