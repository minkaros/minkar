# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Tcl 8.6.16 (Tcl's licence, BSD-style: license.terms; tcl.tk), fetched at
# build time by MANIFEST and checked against its SHA-256: the Tcl Tk 8.6
# needs (gitk and git gui, ../tk). It goes into the same
# /System/Library/Frameworks/Tcl.framework as Tcl 8.5 (../tcl), as
# Versions/8.6, which this directory alone writes; Current stays 8.5, as on
# the reference. tclsh8.6 is inside it, with /usr/bin/tclsh8.6 linking to
# it. The SDK gets the version whole (headers, tclConfig.sh, stub library),
# for Tk's build. Tcl alone: the packages its sources bundle (tdbc, itcl,
# Thread, SQLite) are not built.
#
# Built as ../tcl builds 8.5: configure picks its Darwin build by asking
# uname, which answers as Minkar does; nothing of it runs on the build host.

all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE =		${ROOT_BINARY_DIR}/src-cache
JOBS =		-j${.MAKE.JOBS:U1}
GMAKE =		MAKEFLAGS="" ${TOOLS}/gmake
FW =		System/Library/Frameworks/Tcl.framework
V =		8.6
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
	${GMAKE} ${JOBS} binaries libraries > make.log 2>&1 && \
	${GMAKE} install-binaries install-libraries install-tzdata \
	    install-private-headers INSTALL_ROOT=${STAGE} > install.log 2>&1 || \
	    { tail -n 20 ${.OBJDIR}/build/*.log; exit 1; }
	mv ${STAGE}/usr/bin/tclsh${V} ${STAGE}/${FW}/Versions/${V}/tclsh${V}
	touch ${.TARGET}

# The image: this version of the framework, without what only builds use;
# the SDK: all of it.
${.OBJDIR}/.installed: ${STAGE}/${FW}/Versions/${V}/Tcl
	mkdir -p ${SYSROOT_DIR}/${FW}/Versions/${V} ${MINKAR_SDKROOT}/${FW}/Versions/${V} ${SYSROOT_DIR}/usr/bin
	rsync -rlc --delete --exclude Headers --exclude PrivateHeaders \
	    --exclude tclConfig.sh --exclude 'libtclstub*' --exclude pkgconfig \
	    ${STAGE}/${FW}/Versions/${V}/ ${SYSROOT_DIR}/${FW}/Versions/${V}/
	rsync -rlc --delete ${STAGE}/${FW}/Versions/${V}/ ${MINKAR_SDKROOT}/${FW}/Versions/${V}/
	ln -sf ../../${FW}/Versions/${V}/tclsh${V} ${SYSROOT_DIR}/usr/bin/tclsh${V}
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/tcl8.6 && \
	install -C -m 0444 $$src/license.terms ${SYSROOT_DIR}/usr/share/licenses/tcl8.6/
	touch ${.TARGET}

