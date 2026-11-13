# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CoreMIDI.framework: MIDI clients, ports and endpoints, and packet lists.
# Minkar has no MIDI server or devices yet: clients and ports work, there
# are no sources or destinations, and what is sent goes nowhere.

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
