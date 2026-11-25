# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CoreMIDI.framework (AU8): MIDI clients, ports, devices, entities and
# endpoints, properties and notifications, and packet lists sent between
# processes, through the MIDI server (CoreServices/midiserver).

MK_WERROR = no
FRAMEWORK = CoreMIDI
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = MIDIServices.c
HEADERS = CoreMIDI.h MIDIServices.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
