# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# IOAudioFamily: our own implementation of the audio driver family (BSD-2),
# with its public API. See README.minkar.
MK_WERROR = no
SUBDIR =
KEXT = IOAudioFamily
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.iokit.IOAudioFamily
BUNDLE_VERSION = 300.10

SRCS = IOAudioDevice.cpp IOAudioEngine.cpp IOAudioStream.cpp IOAudioControl.cpp \
	IOAudioEngineUserClient.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

HEADERS = IOAudioTypes.h IOAudioDevice.h IOAudioEngine.h IOAudioStream.h \
	IOAudioControl.h IOAudioLevelControl.h IOAudioToggleControl.h \
	IOAudioEngineUserClient.h

# The headers go in the SDK's Kernel.framework, like the other families';
# IOAudioTypes.h also in IOKit.framework, for user space.
KFW_HEADERS = ${MINKAR_SDKROOT}/System/Library/Frameworks/Kernel.framework/Headers/IOKit/audio
UFW_HEADERS = ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Headers/audio

all: install-headers
install-headers:
	mkdir -p ${KFW_HEADERS} ${UFW_HEADERS}
.for h in ${HEADERS}
	rsync -cL ${.CURDIR}/IOKit/audio/${h} ${KFW_HEADERS}/
.endfor
	rsync -cL ${.CURDIR}/IOKit/audio/IOAudioTypes.h ${UFW_HEADERS}/

.include <minkar.kext.mk>
