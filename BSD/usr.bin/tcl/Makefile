# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Tcl 8.5.19 (Tcl's licence, BSD-style: license.terms; tcl.tk), fetched at
# build time by MANIFEST and checked against its SHA-256. The reference
# system ships Tcl 8.5 as /System/Library/Frameworks/Tcl.framework, and apps
# link it there (Hex Fiend's templates, git's gitk): Minkar installs the
# same framework, version 8.5 (compatibility version 8.5.0), its scripts
# in Resources/Scripts, tclsh8.5 inside it with /usr/bin/tclsh8.5 and
# /usr/bin/tclsh linking to it, and the manuals. The SDK gets the whole
# framework, with its headers, tclConfig.sh and stub library.
#
# Tcl's configure picks its Darwin build (the framework, CoreFoundation)
# by asking uname: it runs with a uname that answers as Minkar does. Its
# install steps run no tclsh, so nothing of it runs on the build host.

all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE =		${ROOT_BINARY_DIR}/src-cache
JOBS =		-j${.MAKE.JOBS:U1}
GMAKE =		MAKEFLAGS="" ${TOOLS}/gmake
FW =		System/Library/Frameworks/Tcl.framework
V =		8.5
STAGE =		${.OBJDIR}/stage

SDK_CFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Qunused-arguments -Wno-nullability-completeness
SDK_LDFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		--ld-path=${LD}

${STAGE}/${FW}/Versions/${V}/Tcl: ${.CURDIR}/MANIFEST ${.CURDIR}/Makefile
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${.OBJDIR}/build ${STAGE} ${.OBJDIR}/fakebin && \
	mkdir -p ${.OBJDIR}/build ${.OBJDIR}/fakebin && \
	printf '#!/bin/sh\ncase "$$1" in -r) echo 19.6.0 ;; -m|-p) echo x86_64 ;; -n) echo minkar ;; *) echo Darwin ;; esac\n' \
	    > ${.OBJDIR}/fakebin/uname && chmod 755 ${.OBJDIR}/fakebin/uname && \
	cd ${.OBJDIR}/build && export PATH=${.OBJDIR}/fakebin:$$PATH && \
	CC="${CC}" AR=${AR} RANLIB=${RANLIB} NM=${NM:Unm} \
	CFLAGS="${SDK_CFLAGS} -O2" LDFLAGS="${SDK_LDFLAGS}" \
	tcl_cv_strtod_buggy=ok tcl_cv_strtod_unbroken=ok \
	$$src/unix/configure --prefix=/usr --mandir=/usr/share/man \
	    --libdir=/System/Library/Frameworks --enable-framework --enable-threads \
	    --build=x86_64-pc-linux-gnu --host=${CpuArch}-apple-darwin19 \
	    > configure.log 2>&1 && \
	${GMAKE} ${JOBS} > make.log 2>&1 && \
	${GMAKE} install-binaries install-libraries install-tzdata install-doc \
	    install-private-headers INSTALL_ROOT=${STAGE} > install.log 2>&1 || \
	    { tail -20 ${.OBJDIR}/build/*.log; exit 1; }
	# tclsh8.5 inside the framework, as the reference keeps it.
	mv ${STAGE}/usr/bin/tclsh${V} ${STAGE}/${FW}/Versions/${V}/tclsh${V}
	ln -sf ../../${FW}/Versions/${V}/tclsh${V} ${STAGE}/usr/bin/tclsh${V}
	ln -sf tclsh${V} ${STAGE}/usr/bin/tclsh
	touch ${.TARGET}

# The image: the framework without what only builds use.
${.OBJDIR}/.installed: ${STAGE}/${FW}/Versions/${V}/Tcl
	mkdir -p ${SYSROOT_DIR}/${FW} ${MINKAR_SDKROOT}/${FW}
	rsync -rlc --delete --exclude Headers --exclude PrivateHeaders \
	    --exclude tclConfig.sh --exclude 'libtclstub*' --exclude pkgconfig \
	    ${STAGE}/${FW}/ ${SYSROOT_DIR}/${FW}/
	rsync -rlc --delete ${STAGE}/${FW}/ ${MINKAR_SDKROOT}/${FW}/
	rsync -rl ${STAGE}/usr/bin/ ${SYSROOT_DIR}/usr/bin/
	rsync -rlc ${STAGE}/usr/share/man/ ${SYSROOT_DIR}/usr/share/man/
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/tcl && \
	install -C -m 0444 $$src/license.terms ${SYSROOT_DIR}/usr/share/licenses/tcl/
	touch ${.TARGET}
