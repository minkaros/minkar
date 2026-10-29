# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# dig, host, nslookup: BIND 9.20.30 (MPL-2.0, dist/COPYRIGHT; isc.org),
# as on macOS. dist/ holds upstream's bind-9.20.30.tar.xz (sha256
# 9e1443a40a72a30272cbc3196d020a6b2e26937111c83e59e101e82e1e3b4fef,
# signature checked against ISC's code-signing key), unchanged. Built
# with its own configure, cross-compiling for Darwin, against libuv
# (BSD/lib/libuv, static), liburcu (BSD/lib/liburcu, shared in /usr/lib)
# and OpenSSL (BSD/usr.bin/openssl), with libedit for nslookup's prompt
# and none of the server's optional libraries. BIND's own libraries
# (it refuses static linking) go to /usr/lib/bind9, private to these
# tools; only the three tools, those libraries and the manuals are
# installed, no named.

all: ${.OBJDIR}/.installed

OBJ =		${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}
OSSL =		${OBJ}/BSD/usr.bin/openssl/stage/usr
UV =		${OBJ}/BSD/lib/libuv/stage
URCU =		${OBJ}/BSD/lib/liburcu/stage/usr
BIND_FLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}

${.OBJDIR}/bin/dig/dig: ${.CURDIR}/Makefile
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist
	env -u CFLAGS -u LDFLAGS \
	CC=${CC} AR=${AR} RANLIB=${RANLIB} LD=${LD} lt_cv_prog_gnu_ld=no \
	PKG_CONFIG=${ROOT_SOURCE_DIR}/tools/build/pkg-config-none.sh \
	LIBUV_CFLAGS="-I${UV}/include" LIBUV_LIBS="${UV}/lib/libuv.a" \
	LIBURCU_CFLAGS="-I${URCU}/include" \
	LIBURCU_LIBS="-L${URCU}/lib -lurcu -lurcu-cds -lurcu-common" \
	OPENSSL_CFLAGS="-I${OSSL}/include" OPENSSL_LIBS="-L${OSSL}/lib -lssl -lcrypto" \
	READLINE_CFLAGS="-I${MINKAR_SDKROOT}/usr/include/editline" READLINE_LIBS="-ledit" \
	CFLAGS="${BIND_FLAGS} -O2 -Wno-nullability-completeness" \
	LDFLAGS="${BIND_FLAGS} -fuse-ld=${LD}" \
	${.CURDIR}/dist/configure --prefix=/usr --sysconfdir=/etc \
	    --localstatedir=/var --libdir=/usr/lib/bind9 \
	    --host=${CpuArch}-apple-darwin19 --disable-doh \
	    --with-readline=libedit --without-libxml2 --without-json-c \
	    --without-zlib --without-lmdb --disable-geoip --without-gssapi \
	    --without-libidn2 --without-jemalloc --disable-dnstap
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -j${.MAKE.JOBS:U1} -C lib
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -j${.MAKE.JOBS:U1} -C bin/dig
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -C doc/man dig.1 host.1 nslookup.1
	rm -rf ${.OBJDIR}/stage
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -C lib install DESTDIR=${.OBJDIR}/stage
	env -u CFLAGS -u LDFLAGS MAKEFLAGS="" ${TOOLS}/gmake -C bin/dig install DESTDIR=${.OBJDIR}/stage

${.OBJDIR}/.installed: ${.OBJDIR}/bin/dig/dig
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/lib/bind9
	for l in ${.OBJDIR}/stage/usr/lib/bind9/*.dylib; do \
	    [ -L $$l ] || install -m 0555 -o 0 -g 0 $$l ${SYSROOT_DIR}/usr/lib/bind9/; \
	done
.for p in dig host nslookup
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/stage/usr/bin/${p} ${SYSROOT_DIR}/usr/bin/${p}
	install -m 0444 ${.OBJDIR}/doc/man/${p}.1 ${SYSROOT_DIR}/usr/share/man/man1/${p}.1
.endfor
	touch ${.TARGET}
