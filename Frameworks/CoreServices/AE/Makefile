# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AE.framework, inside CoreServices.framework as on the reference platform
# (CoreServices re-exports it): Apple event descriptors, lists, records
# and events, their coercions, and handlers for events sent to the same
# process. Foundation links it for NSAppleEventDescriptor.

FRAMEWORK = AE
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
FMWK_INSTALL_DIR = /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks
LDFLAGS += -Wl,-umbrella,CoreServices -Wl,-allowable_client,Foundation -framework CoreFoundation
MK_WERROR = no

SRCS = AEDataModel.c AppleEvents.c
HEADERS = AE.h AEDataModel.h AppleEvents.h
MODULEMAP = AE.modulemap
CFLAGS += -I${.CURDIR} -F${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Frameworks

PARENT = /System/Library/Frameworks/CoreServices.framework
fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}${FMWK_INSTALL_DIR}
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${dir}${FMWK_INSTALL_DIR}
	ln -sfn Versions/Current/Frameworks ${dir}${PARENT}/Frameworks
.endfor

.include <minkar.framework.mk>
