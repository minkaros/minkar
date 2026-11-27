# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# authd: Apple's authorization server (Security, APSL), from the tree in
# Frameworks/Security/OSX/authd, unchanged, in /usr/libexec (in its own
# sandbox profile, com.apple.authd.sb, from the same tree), started by
# launchd when Security.framework's Authorization calls reach its service
# (org.minkar.authd; the maintainer's choice, 2026-11-23: authorization in
# full before the installer). It keeps the rights in /var/db/auth.db, made
# from /System/Library/Security/authorization.plist, and evaluates them;
# the agent Minkar adds asks the user. Frameworks/Security/minkar/authd has
# what it builds with here: the private headers it includes, as what
# Minkar has (no LocalAuthentication, smart cards or audit sessions
# device), and checkpw_internal (the local directory checks a password).

SUBDIR =
MK_WERROR = no
WARNS ?= 0
SEC =	${ROOT_SOURCE_DIR}/Frameworks/Security
AUTHD =	${SEC}/OSX/authd
.PATH: ${AUTHD} ${SEC}/minkar/authd
PROG =	authd
SRCS =	agent.c authdb.c authitems.c authtoken.c authutilities.c ccaudit.c \
	connection.c crc.c credential.c engine.c main.c mechanism.c object.c \
	process.c rule.c server.c session.c checkpw.c
MAN =
SLF =	/System/Library/Frameworks
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -Wno-everything -D__MINKAR__ \
	-include ${AUTHD}/security.auth-Prefix.pch \
	-I${SEC}/minkar/authd/compat -I${AUTHD} \
	-I${SEC}/header_symlinks/macOS -I${SEC}/header_symlinks \
	-I${SEC}/OSX/libsecurity_authorization/lib -I${SEC} \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-F${MINKAR_SDKROOT}${SLF}/OpenDirectory.framework/Versions/A/Frameworks
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -F${MINKAR_SDKROOT}${SLF}/OpenDirectory.framework/Versions/A/Frameworks
LDADD += -framework CoreFoundation -framework Security -framework IOKit \
	-framework OpenDirectory -lsqlite3 -lbsm

# Not optimised: built with usr.sbin's -O2 (5127a96f1f) it ends every
# client's request without an answer (AuthorizationCreate gives
# errAuthorizationInternal, -60008); built without -O it works. The cause
# in the imported sources is not found yet.
CFLAGS += -O0

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/System/Library/LaunchDaemons \
	    ${SYSROOT_DIR}/System/Library/Security
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}
	install -C -m 0644 ${.CURDIR}/org.minkar.authd.plist ${SYSROOT_DIR}/System/Library/LaunchDaemons/
	install -C -m 0644 ${AUTHD}/authorization.plist ${SYSROOT_DIR}/System/Library/Security/
	mkdir -p ${SYSROOT_DIR}/System/Library/Sandbox/Profiles
	install -C -m 0644 ${AUTHD}/com.apple.authd.sb ${SYSROOT_DIR}/System/Library/Sandbox/Profiles/

.include <bsd.prog.mk>
