# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Security.framework: Minkar's own subset of the API (decided 9 Nov 2026),
# beside the APSL Security sources in the parent directory, which are to
# replace it. libcrypto (built in the BSD stage, after the frameworks) is
# opened at run time.

MK_WERROR = no
FRAMEWORK = Security
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
# Authorization is Apple's client (APSL, OSX/libsecurity_authorization),
# which talks to authd (BSD/usr.sbin/authd), with its headers; only
# AuthorizationExecuteWithPrivileges is ours (AuthorizationExecute.c).
SRCS = SecBase.c AuthorizationExecute.c Authorization.c authutilities.c Keychain.c Crypto.c Transform.c Code.c \
	Policy.c Trust.c SecTask.c
HEADERS = Security.h SecBase.h include/Security/Authorization.h include/Security/AuthorizationTags.h \
	include/Security/AuthorizationDB.h include/Security/AuthorizationPlugin.h SecItem.h SecKeychain.h SecCertificate.h SecKey.h SecImportExport.h SecTransform.h SecCode.h SecPolicy.h \
	SecTrust.h SecTask.h
SUBDIR =

APSL_AUTH = ${.CURDIR}/../OSX/libsecurity_authorization/lib
APSL_AUTHD = ${.CURDIR}/../OSX/authd
.PATH: ${APSL_AUTH} ${APSL_AUTHD}
APSL_CFLAGS = -fblocks -Wno-everything -include ${APSL_AUTHD}/security.auth-Prefix.pch \
	-I${.CURDIR}/authd/compat -I${APSL_AUTHD} -I${APSL_AUTH} -I${.CURDIR}/../header_symlinks/macOS \
	-I${.CURDIR}/../header_symlinks -I${.CURDIR}/.. -I${.CURDIR}/../OSX
CFLAGS.Authorization.c = ${APSL_CFLAGS}
CFLAGS.authutilities.c = ${APSL_CFLAGS}

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
# include/Security: the APSL Authorization headers, as <Security/...> while
# the framework itself builds.
CFLAGS += -I${.CURDIR} -I${.CURDIR}/include -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
