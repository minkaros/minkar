# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Terminal (Atlas, A1), in /System/Applications/Utilities. Its emulator
# is libvterm (MIT), fetched by libvterm/MANIFEST (pinned, checked against
# its SHA-256, kept in the source cache) and built into the app; its
# license goes in the app's Resources/Licenses.

SUBDIR =
APP =		Terminal
APP_INSTALL_DIR = /System/Applications/Utilities

VTERM_CACHE =	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
VTERM !=	sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh ${.CURDIR}/libvterm/MANIFEST ${VTERM_CACHE}
VTERM_LICENSES != mkdir -p ${.OBJDIR}/Licenses && cp -f ${VTERM}/LICENSE ${.OBJDIR}/Licenses/libvterm.txt && \
		echo ${.OBJDIR}/Licenses
.PATH:		${VTERM}/src

SRCS =		main.m TMAppDelegate.m TMWindowController.m TMTerminalView.m TMSession.m \
		encoding.c keyboard.c mouse.c parser.c pen.c screen.c state.c unicode.c vterm.c
FRAMEWORKS =	Cocoa
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness \
		-I${VTERM}/include -I${VTERM}/src
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/en.lproj ${.CURDIR}/es.lproj ${.CURDIR}/TerminalHelp.help ${VTERM_LICENSES} \
		${ROOT_SOURCE_DIR}/SystemLibrary/Icons/png/apps/Terminal.png

.include <minkar.app.mk>
