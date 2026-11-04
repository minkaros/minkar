# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# PreferencePanes.framework: NSPreferencePane, the class of System
# Preferences' panes (.prefPane bundles).

SUBDIR =
MK_WERROR = no
FRAMEWORK = PreferencePanes
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = NSPreferencePane.m
HEADERS = NSPreferencePane.h PreferencePanes.h

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -fobjc-arc -F${FMWKS} -Wno-nullability-completeness
LDFLAGS += -F${FMWKS} -framework AppKit -framework Foundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rm -rf ${MINKAR_SDKROOT}/System/Library/Frameworks/${FRAMEWORK}.framework \
	  ${SYSROOT_DIR}/System/Library/Frameworks/${FRAMEWORK}.framework
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
