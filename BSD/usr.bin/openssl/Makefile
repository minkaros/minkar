# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# OpenSSL 3.5.9 (Apache-2.0, dist/LICENSE.txt; openssl-library.org), the
# long-term-support release: /usr/bin/openssl and libcrypto.3.dylib and
# libssl.3.dylib in /usr/lib, with /etc/ssl as its directory (cert.pem,
# openssl.cnf). macOS ships LibreSSL there; Minkar uses OpenSSL (LibreSSL
# keeps the SSLeay licence). dist/ holds upstream's openssl-3.5.9.tar.gz
# (sha256 603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a,
# signature checked against the OpenSSL key) without test/ and demos/,
# which this build does not use. Built with its own Configure
# (darwin64-x86_64, cross-compiling) in a copy in the object folder; the
# headers and libraries also stay in ${.OBJDIR}/stage for the ports that
# use them (BIND's dig), outside the SDK. The manuals of openssl and its
# commands (openssl-req...), as section 1.

all: ${.OBJDIR}/.installed

TC_BIN =	${TOOLCHAIN}/usr/bin
OSSL_CC =	${CC} --sysroot=${MINKAR_SDKROOT} \
		-mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD} \
		-Qunused-arguments -Wno-nullability-completeness
STAGE =		${.OBJDIR}/stage

${.OBJDIR}/build/apps/openssl: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/build ${STAGE}
	cp -R ${.CURDIR}/dist ${.OBJDIR}/build
	cd ${.OBJDIR}/build && env -u CFLAGS -u LDFLAGS -u CXXFLAGS \
	    CC="${OSSL_CC}" AR=${TC_BIN}/llvm-ar RANLIB=${TC_BIN}/llvm-ranlib \
	    perl ./Configure darwin64-x86_64 --prefix=/usr --libdir=lib \
	    --openssldir=/private/etc/ssl shared no-tests
	cd ${.OBJDIR}/build && env -u CFLAGS -u LDFLAGS -u CXXFLAGS MAKEFLAGS="" \
	    ${TOOLS}/gmake -j${.MAKE.JOBS:U1} build_sw
	cd ${.OBJDIR}/build && env -u CFLAGS -u LDFLAGS -u CXXFLAGS MAKEFLAGS="" \
	    ${TOOLS}/gmake install_sw install_ssldirs DESTDIR=${STAGE}
	cd ${.OBJDIR}/build && env -u CFLAGS -u LDFLAGS -u CXXFLAGS MAKEFLAGS="" \
	    ${TOOLS}/gmake install_man_docs DESTDIR=${STAGE}

${.OBJDIR}/.installed: ${.OBJDIR}/build/apps/openssl
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/lib \
	    ${SYSROOT_DIR}/usr/lib/ossl-modules ${SYSROOT_DIR}/private/etc/ssl \
	    ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${STAGE}/usr/bin/openssl ${SYSROOT_DIR}/usr/bin/
.for l in libcrypto libssl
	install -m 0555 -o 0 -g 0 ${STAGE}/usr/lib/${l}.3.dylib ${SYSROOT_DIR}/usr/lib/
.endfor
	install -m 0555 -o 0 -g 0 ${STAGE}/usr/lib/ossl-modules/legacy.dylib \
	    ${SYSROOT_DIR}/usr/lib/ossl-modules/
	install -m 0644 ${STAGE}/private/etc/ssl/openssl.cnf ${SYSROOT_DIR}/private/etc/ssl/
	for m in ${STAGE}/usr/share/man/man1/openssl*.1ossl; do \
	    b=$${m##*/}; install -m 0444 $$m ${SYSROOT_DIR}/usr/share/man/man1/$${b%ossl}; \
	done
	touch ${.TARGET}
