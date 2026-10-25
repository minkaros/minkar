# The curl tool, on /usr/lib/libcurl.4.dylib. No built-in manual.

SUBDIR =
MK_WERROR = no
MAN =
.include "${.CURDIR}/Makefile.inc"
.PATH: ${.CURDIR}/../lib/curlx

PROG = curl
SRCS = ${CURL_CFILES} ${CURLX_CFILES:T}
CURLLIB = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/usr.bin/curl/lib

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/../include -I${.CURDIR}/../lib -I${.CURDIR} \
	-DHAVE_CONFIG_H -O2 -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = ${CURLLIB}/libcurl.4.dylib

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
