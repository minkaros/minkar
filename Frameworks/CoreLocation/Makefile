# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CoreLocation.framework: where the machine is. Minkar has no location
# service (no GPS, no Wi-Fi positioning): location services are off, and a
# manager asked for updates reports kCLErrorDenied to its delegate.

MK_WERROR = no
FRAMEWORK = CoreLocation
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CLLocationManager.m
HEADERS = CoreLocation.h CLLocation.h CLLocationManager.h CLError.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fobjc-arc
LDFLAGS += -F${FMWKS} -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
