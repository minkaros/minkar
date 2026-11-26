# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# midiserver, the MIDI server (AU8): the MIDI setup and its traffic between
# processes, for CoreMIDI (Frameworks/CoreMIDI/MIDIServerProtocol.h).
SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	midiserver
SRCS =	midiserver.c
MAN =
MIDI = ${ROOT_SOURCE_DIR}/Frameworks/CoreMIDI
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -I${MIDI} -F${MINKAR_SDKROOT}/System/Library/Frameworks
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -F${MINKAR_SDKROOT}/System/Library/Frameworks
LDADD += -lbsm -framework CoreMIDI -framework CoreFoundation

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <bsd.prog.mk>
