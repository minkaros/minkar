# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# loginwindow, minimal (WindowServer plan, section 10.3; WS4b): the session's
# owner, with no window of its own until Phase 5. Installed where the
# reference platform keeps it, /System/Library/CoreServices/loginwindow.app.
#
# The login window of ravynOS (LoginWindow.m, DesktopWindow.m, LoginBox.m,
# Label.m, LoadingWindow/ and Makefile.LoginWindow-app) is not built: the
# Phase 5 login window (Atlas) is written anew.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	loginwindow
SRCS =	loginwindow.c
MAN =
LS_FRAMEWORKS = ${MINKAR_SDKROOT}/System/Library/Frameworks/CoreServices.framework/Frameworks
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot -F${LS_FRAMEWORKS}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreServices -lbsm

APP = ${SYSROOT_DIR}/System/Library/CoreServices/loginwindow.app/Contents

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${APP}/MacOS ${APP}/Resources
	install -C -m 0555 -o 0 -g 0 ${PROG} ${APP}/MacOS/${PROG}
	install -C -m 0444 -o 0 -g 0 ${.CURDIR}/loginwindow-Info.plist ${APP}/Info.plist
	install -C -m 0444 -o 0 -g 0 ${.CURDIR}/SessionApplications ${APP}/Resources/SessionApplications

.include <bsd.prog.mk>
