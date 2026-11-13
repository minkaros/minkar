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
SRCS = SecBase.c Authorization.c Keychain.c Crypto.c Transform.c Code.c Policy.c
HEADERS = Security.h SecBase.h Authorization.h AuthorizationDB.h SecItem.h SecKeychain.h SecCertificate.h SecKey.h \
	SecImportExport.h SecTransform.h SecCode.h SecPolicy.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
