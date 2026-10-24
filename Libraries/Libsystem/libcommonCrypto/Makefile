# libcommonCrypto: CommonCrypto's digests (CC_MD5, CC_SHA1, CC_SHA224/256/
# 384/512) and HMAC (CCHmac*), our own (BSD-2), in libSystem as on macOS.
# The headers are the SDK's <CommonCrypto/CommonDigest.h> and
# <CommonCrypto/CommonHMAC.h>. Apple's CommonCrypto (../CommonCrypto, on
# corecrypto) is not built. Host tests: make -C tests.

SUBDIR =
MK_WERROR = no
LIB = commonCrypto
SRCS = digest.c hmac.c

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -Wno-deprecated-declarations

LDFLAGS = -Wl,-syslibroot,${RAVYN_SDKROOT}/usr/lib -umbrella System \
	-L${RAVYN_SDKROOT}/usr/lib/system \
	-Wl,-current_version,60165 -Wl,-compatibility_version,1 \
	-Wl,-install_name,/usr/lib/system/libcommonCrypto.dylib \
	-mmacos-version-min=${MACOS_VERSION_MIN} \
	-nodefaultlibs -lsystem_c

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${RAVYN_SDKROOT}/usr/lib/system ${SYSROOT_DIR}/usr/lib/system
	cp -f ${.TARGET} ${RAVYN_SDKROOT}/usr/lib/system/
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/system/

.include <bsd.lib.mk>
