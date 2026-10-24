# libcurl: /usr/lib/libcurl.4.dylib (as on macOS), Mbed TLS and zlib
# linked in; only the curl_* API is exported.

SUBDIR =
MK_WERROR = no
.include "${.CURDIR}/Makefile.inc"
.PATH: ${.CURDIR}/vauth ${.CURDIR}/vtls ${.CURDIR}/vquic ${.CURDIR}/vssh \
	${.CURDIR}/curlx

LIB = curl
SRCS = ${CSOURCES:T}

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/../include -I${.CURDIR} -I${RAVYN_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/zlib \
	-DHAVE_CONFIG_H -DBUILDING_LIBCURL -DCURL_HIDDEN_SYMBOLS \
	-fvisibility=hidden -fPIC -O2 -Wno-nullability-completeness

LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libcurl.4.dylib \
	-Wl,-current_version,9.0.0 -Wl,-compatibility_version,7.0.0 \
	-Wl,-exported_symbol,_curl_* \
	${RAVYN_SDKROOT}/usr/local/lib/libmbedtls.a \
	${RAVYN_SDKROOT}/usr/lib/libz.a -lSystem

all: ${.OBJDIR}/libcurl.4.dylib

${.OBJDIR}/libcurl.4.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
.for d in ${RAVYN_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib
	cp -f ${.TARGET} ${d}/usr/lib/
	ln -sf libcurl.4.dylib ${d}/usr/lib/libcurl.dylib
.endfor
	mkdir -p ${RAVYN_SDKROOT}/usr/include/curl
	cp -f ${.CURDIR}/../include/curl/*.h ${RAVYN_SDKROOT}/usr/include/curl/

.include <bsd.lib.mk>
