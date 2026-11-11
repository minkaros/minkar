# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Ruby 3.3.9 (Ruby licence or BSD-2-Clause, COPYING and BSDL; ruby-lang.org),
# fetched at build time by MANIFEST and checked against its SHA-256: the
# reference system ships a Ruby interpreter, and so does Minkar (the
# maintainer's choice, 2026-11-11). Installs /usr/bin/ruby and its tools
# (irb, erb, gem, bundle, rake...), /usr/lib/libruby.3.3.dylib, the standard
# library in /usr/lib/ruby and the headers in the SDK, for gems with C code.
#
# Cross-compiling needs a Ruby of the same version on the build host: it is
# built first from the same source into ${HOST_RUBY}. The extensions use
# zlib, OpenSSL (BSD/usr.bin/openssl, in the SDK's usr/local), libffi
# (fiddle; WebKit's, in /usr/lib/webkit) and libyaml (psych), which is
# built first as a static library from MANIFEST.libyaml (MIT) and linked
# into psych.

all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE =		${ROOT_BINARY_DIR}/src-cache
RUBY_VERSION !=	awk '$$1 == "version" { print $$2 }' ${.CURDIR}/MANIFEST
HOST_RUBY =	${ROOT_BINARY_DIR}/extlib-host/ruby-${RUBY_VERSION}
JOBS =		-j${.MAKE.JOBS:U1}
GMAKE =		MAKEFLAGS="" ${TOOLS}/gmake

SDK_CFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Qunused-arguments -Wno-nullability-completeness \
		-I${MINKAR_SDKROOT}/usr/local/include \
		-I${MINKAR_SDKROOT}/usr/local/webkit/include
SDK_LDFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		--ld-path=${LD} -L${MINKAR_SDKROOT}/usr/local/lib \
		-L${MINKAR_SDKROOT}/usr/lib/webkit

# The host's Ruby, for the build only: its own compiler and libraries.
${HOST_RUBY}/bin/ruby: ${.CURDIR}/MANIFEST
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${.OBJDIR}/host && mkdir -p ${.OBJDIR}/host && cd ${.OBJDIR}/host && \
	env -i PATH=/usr/bin:/bin HOME=${.OBJDIR}/host \
	    $$src/configure --prefix=${HOST_RUBY} --disable-install-doc \
	    --disable-install-rdoc --without-gmp > configure.log 2>&1 && \
	env -i PATH=/usr/bin:/bin HOME=${.OBJDIR}/host ${TOOLS}/gmake ${JOBS} && \
	env -i PATH=/usr/bin:/bin HOME=${.OBJDIR}/host ${TOOLS}/gmake install > install.log 2>&1 || exit 1

# libyaml for psych, static: psych's own way of building it from source
# gives libyaml's configure the compiler without the SDK.
LIBYAML =	${.OBJDIR}/libyaml-stage
${LIBYAML}/lib/libyaml.a: ${.CURDIR}/MANIFEST.libyaml ${.CURDIR}/Makefile
	yaml=$$(${FETCH} ${.CURDIR}/MANIFEST.libyaml ${CACHE}) && \
	rm -rf ${.OBJDIR}/libyaml ${LIBYAML} && mkdir -p ${.OBJDIR}/libyaml && \
	cd ${.OBJDIR}/libyaml && \
	CC="${CC}" AR=${AR} RANLIB=${RANLIB} \
	CFLAGS="${SDK_CFLAGS} -O2 -fPIC" LDFLAGS="${SDK_LDFLAGS}" \
	$$yaml/configure --prefix=${LIBYAML} \
	    --build=x86_64-pc-linux-gnu --host=${CpuArch}-apple-darwin19 \
	    --disable-shared --enable-static > configure.log 2>&1 && \
	${GMAKE} ${JOBS} && ${GMAKE} install > install.log 2>&1 || exit 1

${.OBJDIR}/stage/usr/bin/ruby: ${HOST_RUBY}/bin/ruby ${LIBYAML}/lib/libyaml.a ${.CURDIR}/Makefile
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${.OBJDIR}/target ${.OBJDIR}/stage && mkdir -p ${.OBJDIR}/target && \
	cd ${.OBJDIR}/target && \
	export PKG_CONFIG_LIBDIR=${LIBYAML}/lib/pkgconfig PKG_CONFIG_PATH= && \
	CC="${CC}" CXX="${CXX}" AR=${AR} RANLIB=${RANLIB} NM=${NM:Unm} \
	CFLAGS="${SDK_CFLAGS} -O2" CPPFLAGS="${SDK_CFLAGS}" LDFLAGS="${SDK_LDFLAGS}" \
	$$src/configure --prefix=/usr \
	    --build=x86_64-pc-linux-gnu --host=${CpuArch}-apple-darwin19 \
	    --with-baseruby=${HOST_RUBY}/bin/ruby \
	    --enable-shared --disable-install-doc --disable-install-rdoc \
	    --disable-dtrace --without-gmp --disable-jit-support \
	    --with-openssl-dir=${MINKAR_SDKROOT}/usr/local \
	    --with-libyaml-dir=${LIBYAML} \
	    --with-out-ext=win32,win32ole,dbm,gdbm > configure.log 2>&1 && \
	${GMAKE} ${JOBS} && \
	${GMAKE} install DESTDIR=${.OBJDIR}/stage > install.log 2>&1 || exit 1

${.OBJDIR}/.installed: ${.OBJDIR}/stage/usr/bin/ruby
	rsync -rlc ${.OBJDIR}/stage/usr/bin/ ${SYSROOT_DIR}/usr/bin/
	rsync -rlc --exclude pkgconfig ${.OBJDIR}/stage/usr/lib/ ${SYSROOT_DIR}/usr/lib/
	rsync -rlc ${.OBJDIR}/stage/usr/share/man/ ${SYSROOT_DIR}/usr/share/man/
	mkdir -p ${MINKAR_SDKROOT}/usr/include ${MINKAR_SDKROOT}/usr/lib
	rsync -rlc ${.OBJDIR}/stage/usr/include/ ${MINKAR_SDKROOT}/usr/include/
	rsync -rlc ${.OBJDIR}/stage/usr/lib/libruby* ${MINKAR_SDKROOT}/usr/lib/
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/ruby && \
	install -C -m 0444 $$src/COPYING $$src/BSDL $$src/LEGAL \
	    ${SYSROOT_DIR}/usr/share/licenses/ruby/
	touch ${.TARGET}
