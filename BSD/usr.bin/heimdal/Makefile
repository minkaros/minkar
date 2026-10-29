# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# kinit, klist, kdestroy: Heimdal 7.8.0 (BSD-3, dist/LICENSE; h5l.org),
# the Kerberos 5 macOS uses. dist/ holds the release tarball
# (sha256 fd87a207846fa650fd377219adc4b8a8193e55904d8a752c2c3715b4155d8d38,
# signature checked against Heimdal Security's key); the changes, in
# kuser/kinit.c and lib/krb5/krb5_locl.h, are marked Minkar.
#
# Heimdal builds with tools it makes itself (asn1_compile, slc,
# compile_et): they are built first for the build machine, in
# ${.OBJDIR}/host, and its configure takes them with --with-cross-tools
# for the Darwin build in ${.OBJDIR}/cross. Static libraries, the
# internal hcrypto, and none of the optional parts (OTP, KCM, AFS, the
# KDC databases, LDAP, digest, kx509); PKINIT stays (7.8 does not link
# without it). Installs kinit, kdestroy
# and klist (heimtools, which acts by the name it is run as) with their
# manuals. What configure runs on the target is answered for Darwin, and
# hcrypto uses its own algorithms, not CommonCrypto (which lacks MD4).
# Credentials go to a file cache (FILE:/tmp/krb5cc_<uid>): the API cache
# Heimdal defaults to on Apple systems needs a service Minkar lacks.

all: ${.OBJDIR}/.installed

HFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}
H_OPTS =	--disable-shared --enable-static --without-openssl \
		--disable-otp --disable-kcm --disable-afs-support \
		--disable-ndbm-db --disable-mdb-db --without-berkeley-db \
		--disable-sqlite-cache --without-sqlite3 --without-ldap \
		--disable-digest --disable-kx509 \
		--without-readline --without-libedit --without-x \
		--disable-heimdal-documentation --without-libintl
H_DARWIN =	ac_cv_func_snprintf_working=yes ac_cv_func_vsnprintf_working=yes \
		ac_cv_func_realloc_broken=no ac_cv___function__=yes \
		krb_cv_c_bigendian=no ac_cv_func_getpwnam_r_posix=yes \
		ac_cv_func_getcwd_broken=no \
		ac_cv_header_CommonCrypto_CommonDigest_h=no \
		ac_cv_header_CommonCrypto_CommonCryptor_h=no
GM =		env -u CFLAGS -u LDFLAGS -u CXXFLAGS -u MACOSX_DEPLOYMENT_TARGET \
		MAKEFLAGS="" ${TOOLS}/gmake

${.OBJDIR}/xtools/asn1_compile: ${.CURDIR}/Makefile
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist
	rm -rf ${.OBJDIR}/host ${.OBJDIR}/xtools
	mkdir -p ${.OBJDIR}/host ${.OBJDIR}/xtools
	cd ${.OBJDIR}/host && env -u CFLAGS -u LDFLAGS -u CXXFLAGS \
	    -u MACOSX_DEPLOYMENT_TARGET CC=cc PKG_CONFIG=false \
	    ${.CURDIR}/dist/configure ${H_OPTS}
	cd ${.OBJDIR}/host && ${GM} -j${.MAKE.JOBS:U1} -C include
.for d in lib/roken lib/vers lib/com_err lib/libedit lib/sl lib/asn1
	cd ${.OBJDIR}/host && ${GM} -j${.MAKE.JOBS:U1} -C ${d}
.endfor
	cp ${.OBJDIR}/host/lib/asn1/asn1_compile ${.OBJDIR}/host/lib/sl/slc \
	    ${.OBJDIR}/host/lib/com_err/compile_et ${.OBJDIR}/xtools/

${.OBJDIR}/cross/kuser/kinit: ${.OBJDIR}/xtools/asn1_compile
	rm -rf ${.OBJDIR}/cross
	mkdir -p ${.OBJDIR}/cross
	cd ${.OBJDIR}/cross && env -u CFLAGS -u LDFLAGS -u CXXFLAGS \
	    -u MACOSX_DEPLOYMENT_TARGET \
	    CC=${CC} AR=${AR} RANLIB=${RANLIB} LD=${LD} lt_cv_prog_gnu_ld=no \
	    PKG_CONFIG=false ${H_DARWIN} \
	    CFLAGS="${HFLAGS} -O2 -Wno-nullability-completeness -Wno-error -D__MINKAR__" \
	    LDFLAGS="${HFLAGS} -fuse-ld=${LD}" LIBS="-framework CoreFoundation" \
	    ${.CURDIR}/dist/configure --prefix=/usr --sysconfdir=/etc \
	    --host=${CpuArch}-apple-darwin19 \
	    --with-cross-tools=${.OBJDIR}/xtools/ ${H_OPTS}
	cd ${.OBJDIR}/cross && ${GM} -j${.MAKE.JOBS:U1} -C include
	cd ${.OBJDIR}/cross && ${GM} -j${.MAKE.JOBS:U1} -C lib
	cd ${.OBJDIR}/cross && ${GM} -j${.MAKE.JOBS:U1} -C kuser kinit kdestroy heimtools

${.OBJDIR}/.installed: ${.OBJDIR}/cross/kuser/kinit
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/cross/kuser/kinit ${SYSROOT_DIR}/usr/bin/kinit
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/cross/kuser/kdestroy ${SYSROOT_DIR}/usr/bin/kdestroy
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/cross/kuser/heimtools ${SYSROOT_DIR}/usr/bin/klist
.for m in kinit klist kdestroy
	install -m 0444 ${.CURDIR}/dist/kuser/${m}.1 ${SYSROOT_DIR}/usr/share/man/man1/${m}.1
.endfor
	touch ${.TARGET}
