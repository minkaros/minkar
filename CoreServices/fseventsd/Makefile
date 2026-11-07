# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# fseventsd (Atlas.md, K0): the file system's events, from the
# kernel's /dev/fsevents, for FSEvents' streams (fseventsd.defs in
# Frameworks/CoreServices/FSEvents).

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	fseventsd
SRCS =	fseventsd.c fseventsdServer.c
MAN =
FSE =	${ROOT_SOURCE_DIR}/Frameworks/CoreServices/FSEvents
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -Wno-nullability-completeness -I${.OBJDIR} -I${FSE}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm -framework CoreFoundation
CLEANFILES += fseventsdServer.c fseventsdServer.h sys/fsevents.h

# The kernel's interface to /dev/fsevents, which the SDK does not carry.
sys/fsevents.h: ${ROOT_SOURCE_DIR}/Kernel/xnu/bsd/sys/fsevents.h
	mkdir -p sys
	cp -f ${.ALLSRC} ${.TARGET}

# The server side of the MIG interface.
fseventsdServer.c fseventsdServer.h: ${FSE}/fseventsd.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${FSE} \
	    -header /dev/null -user /dev/null -server fseventsdServer.c \
	    -sheader fseventsdServer.h ${.ALLSRC}

fseventsd.o: fseventsdServer.h sys/fsevents.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}
	install -d -m 0755 -o 0 -g 0 ${SYSROOT_DIR}/private/var/db/fseventsd

.include <bsd.prog.mk>
