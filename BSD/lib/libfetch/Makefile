# libfetch (FreeBSD, BSD-3) from git history, for fetch(1): HTTP, HTTPS,
# FTP and file URLs. TLS is our own fetch_tls.c (BSD-2) on Mbed TLS in
# place of OpenSSL (WITH_MBEDTLS); digest auth uses libmd's MD5.
# Static library: macOS has no libfetch.

SUBDIR =
MK_WERROR = no
LIB = fetch
SRCS = fetch.c common.c ftp.c http.c file.c fetch_tls.c
DPSRCS = ftperr.h httperr.h
CLEANFILES = ftperr.h httperr.h

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I. -I${.CURDIR} -I${RAVYN_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libmd \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-DINET6 -DWITH_MBEDTLS -DFTP_COMBINE_CWDS -Wno-nullability-completeness

all: lib${LIB}.a

${SRCS:M*.c:R:S/$/.o/}: ftperr.h httperr.h

ftperr.h: ftp.errors ${.CURDIR}/Makefile
	@echo "static struct fetcherr ftp_errlist[] = {" > ${.TARGET}
	@cat ${.CURDIR}/ftp.errors \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

httperr.h: http.errors ${.CURDIR}/Makefile
	@echo "static struct fetcherr http_errlist[] = {" > ${.TARGET}
	@cat ${.CURDIR}/http.errors \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown HTTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

.include <bsd.lib.mk>
