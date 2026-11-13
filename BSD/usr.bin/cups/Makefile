# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CUPS (OpenPrinting, Apache-2.0): for now its client library,
# /usr/lib/libcups.2.dylib, and its headers in the SDK, as the reference
# platform has them (decided 13 Nov 2026). There is no cupsd yet, so no
# printers: apps print to PDF. TLS through OpenSSL (the reference
# platform's own TLS is not Minkar's), printers found by DNS-SD
# (mDNSResponder). Fetched by MANIFEST and built in a copy of the tree
# (CUPS builds in its own tree).

SUBDIR =
all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC_BIN =	${TOOLCHAIN}/usr/bin
CUPS_CC =	${CC} --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-fuse-ld=${LD} -Qunused-arguments -Wno-nullability-completeness
BUILD =		${.OBJDIR}/build
STAGE =		${.OBJDIR}/stage

${BUILD}/cups/libcups.2.dylib: ${.CURDIR}/Makefile ${.CURDIR}/MANIFEST
	rm -rf ${BUILD} ${STAGE}
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && cp -R $$src ${BUILD}
	cd ${BUILD} && env -u CFLAGS -u LDFLAGS -u CXXFLAGS MAKEFLAGS="" \
	    CC="${CUPS_CC}" CXX="${CUPS_CC:S/clang/clang++/}" \
	    AR=${TC_BIN}/llvm-ar RANLIB=${TC_BIN}/llvm-ranlib STRIP=${TC_BIN}/llvm-strip \
	    CPPFLAGS="-I${MINKAR_SDKROOT}/usr/local/include" \
	    LDFLAGS="-L${MINKAR_SDKROOT}/usr/local/lib" \
	    PKG_CONFIG=false \
	    ./configure --host=x86_64-apple-darwin19 --prefix=/usr \
	    --sysconfdir=/private/etc --localstatedir=/private/var \
	    --with-tls=openssl --with-dnssd=mdnsresponder --disable-gssapi \
	    --disable-pam --disable-dbus --disable-libusb --disable-acl > ${.OBJDIR}/configure.log 2>&1 || \
	    { tail -30 ${.OBJDIR}/configure.log; exit 1; }
	cd ${BUILD} && env -u CFLAGS -u LDFLAGS -u CXXFLAGS MAKEFLAGS="" \
	    ${TOOLS}/gmake -j${.MAKE.JOBS:U1} libs > ${.OBJDIR}/build.log 2>&1 || \
	    { grep -B2 -A6 'error' ${.OBJDIR}/build.log | head -40; exit 1; }

${.OBJDIR}/.installed: ${BUILD}/cups/libcups.2.dylib
	mkdir -p ${SYSROOT_DIR}/usr/lib ${MINKAR_SDKROOT}/usr/lib ${MINKAR_SDKROOT}/usr/include/cups \
	    ${SYSROOT_DIR}/usr/share/doc/cups
	install -C -m 0555 -o 0 -g 0 ${BUILD}/cups/libcups.2.dylib ${SYSROOT_DIR}/usr/lib/
	rsync -cL ${BUILD}/cups/libcups.2.dylib ${MINKAR_SDKROOT}/usr/lib/
	ln -sf libcups.2.dylib ${MINKAR_SDKROOT}/usr/lib/libcups.dylib
.for h in adminutil.h array.h cups.h dir.h file.h http.h ipp.h language.h pwg.h \
	ppd.h raster.h sidechannel.h transcode.h versioning.h backend.h
	rsync -cL ${BUILD}/cups/${h} ${MINKAR_SDKROOT}/usr/include/cups/
.endfor
	install -C -m 0444 ${BUILD}/LICENSE ${BUILD}/NOTICE ${SYSROOT_DIR}/usr/share/doc/cups/
	touch ${.TARGET}

clean: .PHONY
	rm -rf ${BUILD} ${STAGE} ${.OBJDIR}/*.log ${.OBJDIR}/.installed

.include <bsd.prog.mk>
