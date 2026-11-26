# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Authorization Agent: the user's side and the privileged side of
# authorization, for authd (the maintainer's choice, 2026-11-23): Atlas's
# window asking for an administrator's name and password, and their check
# in the local directory. launchd starts it when authd reaches one of its
# services (org.minkar.authorizationagent).

SUBDIR =
APP =		AuthorizationAgent
APP_INSTALL_DIR = /System/Library/CoreServices
SRCS =		main.m
FRAMEWORKS =	AppKit Foundation OpenDirectory
MK_WERROR =	no
# XPC's objects are libxpc's, not Objective-C objects ARC could manage.
CFLAGS +=	-fobjc-arc -DOS_OBJECT_USE_OBJC=0 -g -std=gnu17 -Wno-incompatible-sysroot \
		-Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/es.lproj

all: launchd-job

launchd-job: .PHONY
	mkdir -p ${SYSROOT_DIR}/System/Library/LaunchDaemons
	install -C -m 0644 ${.CURDIR}/org.minkar.authorizationagent.plist ${SYSROOT_DIR}/System/Library/LaunchDaemons/

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.app.mk>
