# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ScriptingBridge.framework: controlling other applications through their
# scripting dictionaries (SBApplication, SBObject, SBElementArray). VLC's
# and UTM's interfaces link it. Minkar has no Open Scripting Architecture
# yet (Phase 6, osascript), so no application can be reached: the
# constructors are noisy stubs that return nil, as for an application
# that is not there.

MK_WERROR = no
FRAMEWORK = ScriptingBridge
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = SBObject.m SBElementArray.m SBApplication.m
HEADERS = ScriptingBridge.h SBObject.h SBElementArray.h SBApplication.h
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
