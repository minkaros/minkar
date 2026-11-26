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
SRCS = AudioHardware.c AudioDevices.c AudioDeviceIO.c AudioAggregate.c AudioServerUser.c HostTime.c \
  CATapDescription.m AudioTap.m
HEADERS = CoreAudio.h CoreAudioTypes.h AudioHardware.h HostTime.h AudioServerPlugIn.h CATapDescription.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -I${.CURDIR} -I${.OBJDIR} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework Foundation -framework CoreFoundation -framework IOKit -lobjc

# The audio service's interface, its client side (AudioDeviceIO.c).
CLEANFILES += AudioServerUser.c AudioServerUser.h
AudioServerUser.c AudioServerUser.h: ${.CURDIR}/AudioServer.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${.CURDIR} \
	    -user AudioServerUser.c -header AudioServerUser.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}

AudioDeviceIO.o AudioDevices.o AudioTap.o: AudioServerUser.h

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.framework.mk>
