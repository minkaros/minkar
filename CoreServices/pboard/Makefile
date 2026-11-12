# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pboard, the pasteboard server (Phase 4): named pasteboards by user. Its
# interface is in Frameworks/AppKit/Pasteboard; AppKit's NSPasteboard and
# pbcopy/pbpaste are its clients.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	pboard
SRCS =	pboard.c PasteboardServer.c
MAN =
PB = ${ROOT_SOURCE_DIR}/Frameworks/AppKit/Pasteboard
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -I${.OBJDIR} -I${PB}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm -framework CoreFoundation
CLEANFILES += PasteboardServer.c PasteboardServer.h

# The server side of the MIG interface.
PasteboardServer.c PasteboardServer.h: ${PB}/Pasteboard.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${PB} \
	    -header /dev/null -user /dev/null -server PasteboardServer.c \
	    -sheader PasteboardServer.h ${.ALLSRC}

pboard.o: PasteboardServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}
	# Its log directory: launchd opens the job's StandardErrorPath as
	# nobody (UID and GID -2), who cannot write /var/log.
	install -d -m 0755 -o 4294967294 -g 4294967294 ${SYSROOT_DIR}/private/var/log/pboard

.include <bsd.prog.mk>
