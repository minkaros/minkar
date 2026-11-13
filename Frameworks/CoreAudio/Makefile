# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CoreAudio.framework (AU2): the audio objects (the system, its devices
# and streams) and their properties, and host time. Until the audio service
# and its devices (AU1, AU2) exist, the system object has no devices.

MK_WERROR = no
FRAMEWORK = CoreAudio
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = AudioHardware.c HostTime.c
HEADERS = CoreAudio.h CoreAudioTypes.h AudioHardware.h HostTime.h
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
