# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Python 3.13.18 (PSF-2.0, LICENSE; python.org), fetched at build time by
# MANIFEST and checked against its SHA-256 (signature checked against
# Thomas Wouters's key, the 3.13 release manager): the reference system's
# scripts and apps expect /usr/bin/python3, and Minkar has it (the
# maintainer's choice, 2026-11-11). Installs python3, python3.13 and
# pydoc3 (not idle3, which needs Tk), libpython3.13.dylib in /usr/lib, the
# standard library in /usr/lib/python3.13, the headers in the SDK, the
# licence in /usr/share/licenses/python. No pip: python3 -m ensurepip
# installs it.
#
# Cross-compiling needs a Python of the same version on the build host,
# built first from the same source into ${HOST_PYTHON}, and a patch for
# configure to take Darwin as a cross-build host (patches/). The modules
# use the SDK's zlib, OpenSSL (BSD/usr.bin/openssl), libffi (ctypes;
# WebKit's), SQLite, ncurses and libedit (readline); Expat and mpdecimal
# are Python's own copies. bz2 and lzma wait for libbz2 and liblzma's
# headers in the SDK.

all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE =		${ROOT_BINARY_DIR}/src-cache
PY_VERSION !=	awk '$$1 == "version" { print $$2 }' ${.CURDIR}/MANIFEST
HOST_PYTHON =	${ROOT_BINARY_DIR}/extlib-host/python-${PY_VERSION}
JOBS =		-j${.MAKE.JOBS:U1}
GMAKE =		MAKEFLAGS="" ${TOOLS}/gmake
SDK =		${MINKAR_SDKROOT}

SDK_CFLAGS =	--sysroot=${SDK} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Qunused-arguments -Wno-nullability-completeness
SDK_LDFLAGS =	--sysroot=${SDK} -mmacos-version-min=${MACOS_VERSION_MIN} \
		--ld-path=${LD}

# What configure cannot find out when cross-compiling, as on the reference
# system.
PY_DARWIN =	ac_cv_file__dev_ptmx=yes ac_cv_file__dev_ptc=no \
		ac_cv_buggy_getaddrinfo=no

# The host's Python, for the build only.
${HOST_PYTHON}/bin/python3: ${.CURDIR}/MANIFEST
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${.OBJDIR}/host && mkdir -p ${.OBJDIR}/host && cd ${.OBJDIR}/host && \
	env -i PATH=/usr/bin:/bin HOME=${.OBJDIR}/host \
	    $$src/configure --prefix=${HOST_PYTHON} --without-ensurepip \
	    > configure.log 2>&1 && \
	env -i PATH=/usr/bin:/bin HOME=${.OBJDIR}/host ${TOOLS}/gmake ${JOBS} > build.log 2>&1 && \
	env -i PATH=/usr/bin:/bin HOME=${.OBJDIR}/host ${TOOLS}/gmake install > install.log 2>&1 || exit 1

${.OBJDIR}/stage/usr/bin/python3: ${HOST_PYTHON}/bin/python3 ${.CURDIR}/Makefile \
    ${.CURDIR}/patches/0001-darwin-cross-build.patch
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${.OBJDIR}/src ${.OBJDIR}/target ${.OBJDIR}/stage && \
	cp -a $$src ${.OBJDIR}/src && \
	for p in ${.CURDIR}/patches/*.patch; do \
	    patch -s -d ${.OBJDIR}/src -p1 < $$p || exit 1; done && \
	mkdir -p ${.OBJDIR}/target && cd ${.OBJDIR}/target && \
	export PKG_CONFIG_LIBDIR=${.OBJDIR}/no-pkgconfig PKG_CONFIG_PATH= && \
	CC="${CC}" CXX="${CXX}" AR=${AR} RANLIB=${RANLIB} READELF=: \
	CFLAGS="${SDK_CFLAGS} -O2" CPPFLAGS="${SDK_CFLAGS}" LDFLAGS="${SDK_LDFLAGS}" \
	MACOSX_DEPLOYMENT_TARGET=${MACOS_VERSION_MIN} ${PY_DARWIN} \
	ZLIB_CFLAGS= ZLIB_LIBS=-lz \
	LIBFFI_CFLAGS=-I${SDK}/usr/local/webkit/include \
	LIBFFI_LIBS="-L${SDK}/usr/lib/webkit -lffi" \
	LIBSQLITE3_CFLAGS= LIBSQLITE3_LIBS=-lsqlite3 \
	CURSES_CFLAGS= CURSES_LIBS=-lncurses PANEL_CFLAGS= PANEL_LIBS=-lpanel \
	LIBEDIT_CFLAGS= LIBEDIT_LIBS=-ledit \
	LIBUUID_CFLAGS= LIBUUID_LIBS= \
	${.OBJDIR}/src/configure --prefix=/usr \
	    --build=x86_64-pc-linux-gnu --host=${CpuArch}-apple-darwin19 \
	    --with-build-python=${HOST_PYTHON}/bin/python3 \
	    --enable-shared --without-ensurepip --with-readline=editline \
	    --with-openssl=${SDK}/usr/local --with-system-expat=no \
	    --with-system-libmpdec=no --disable-test-modules \
	    > configure.log 2>&1 && \
	${GMAKE} ${JOBS} > build.log 2>&1 && \
	${GMAKE} install DESTDIR=${.OBJDIR}/stage > install.log 2>&1 || exit 1

${.OBJDIR}/.installed: ${.OBJDIR}/stage/usr/bin/python3
	rsync -rlc --exclude 'idle*' --exclude '*-config' ${.OBJDIR}/stage/usr/bin/ ${SYSROOT_DIR}/usr/bin/
	rsync -rlc --exclude pkgconfig --exclude 'idlelib' --exclude 'tkinter' \
	    --exclude 'turtledemo' --exclude '__pycache__' \
	    ${.OBJDIR}/stage/usr/lib/ ${SYSROOT_DIR}/usr/lib/
	rsync -rlc ${.OBJDIR}/stage/usr/share/man/ ${SYSROOT_DIR}/usr/share/man/
	mkdir -p ${SDK}/usr/include ${SDK}/usr/lib
	rsync -rlc ${.OBJDIR}/stage/usr/include/ ${SDK}/usr/include/
	rsync -rlc ${.OBJDIR}/stage/usr/lib/libpython* ${SDK}/usr/lib/
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/python
	install -C -m 0444 ${.OBJDIR}/src/LICENSE ${SYSROOT_DIR}/usr/share/licenses/python/
	touch ${.TARGET}
