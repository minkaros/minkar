# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# libcommonCrypto, our own (BSD-2), in libSystem as on macOS:
#   digests     CC_MD5, CC_SHA1, CC_SHA224/256/384/512    (digest.c)
#   HMAC        CCHmac*                                   (hmac.c)
#   ciphers     CCCryptor*, CCCrypt: AES in ECB, CBC, CTR, CFB, CFB8, OFB;
#               RC4. DES, 3DES, CAST, RC2, Blowfish: kCCUnimplemented
#                                                         (cryptor.c, aes.c)
#   random      CCRandomGenerateBytes                     (keyderivation.c)
#   PBKDF2      CCKeyDerivationPBKDF, CCCalibratePBKDF    (keyderivation.c)
# The headers are the SDK's <CommonCrypto/...>. Apple's CommonCrypto
# (../CommonCrypto, on corecrypto) is not built. Tables: gen_tables.py.
# Host tests: make -C tests.

SUBDIR =
MK_WERROR = no
LIB = commonCrypto
SRCS = digest.c hmac.c aes.c cryptor.c keyderivation.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -Wno-deprecated-declarations -Wno-nullability-completeness

LDFLAGS = -Wl,-syslibroot,${MINKAR_SDKROOT}/usr/lib -umbrella System \
	-L${MINKAR_SDKROOT}/usr/lib/system \
	-Wl,-current_version,60165 -Wl,-compatibility_version,1 \
	-Wl,-install_name,/usr/lib/system/libcommonCrypto.dylib \
	-mmacos-version-min=${MACOS_VERSION_MIN} \
	-nodefaultlibs -lsystem_c -lsystem_malloc -lsystem_kernel -ldyld \
	-Wl,-unexported_symbol,__cc_*

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib/system ${SYSROOT_DIR}/usr/lib/system
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/system/
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/system/

.include <bsd.lib.mk>
