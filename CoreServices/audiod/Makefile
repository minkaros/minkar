# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# audiod, the audio service (AU2): it owns the output devices and mixes
# its clients into them (AudioServer.defs, in CoreAudio).
SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	audiod
SRCS =	audiod.c plugins.c AudioServerServer.c
MAN =
CA = ${ROOT_SOURCE_DIR}/Frameworks/CoreAudio
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -I${.OBJDIR} -I${CA}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lbsm -framework IOKit -framework CoreFoundation
CLEANFILES += AudioServerServer.c AudioServerServer.h

AudioServerServer.c AudioServerServer.h: ${CA}/AudioServer.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${CA} \
	    -header /dev/null -user /dev/null -server AudioServerServer.c \
	    -sheader AudioServerServer.h ${.ALLSRC}

audiod.o: AudioServerServer.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <bsd.prog.mk>
