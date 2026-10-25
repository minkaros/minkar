# fetch (FreeBSD, BSD-3) on libfetch, with Mbed TLS for https.

SUBDIR =
MK_WERROR = no
MAN =
PROG = fetch
FETCHLIB = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libfetch
MDLIB = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libfetch \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-include sys/ioctl.h -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = ${FETCHLIB}/libfetch.a ${MINKAR_SDKROOT}/usr/local/lib/libmbedtls.a \
	${MDLIB}/libmd.a \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
