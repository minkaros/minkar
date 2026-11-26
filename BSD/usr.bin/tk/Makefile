# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Tk 8.6.16 (Tk's licence, BSD-style: license.terms; tcl.tk), fetched at
# build time by MANIFEST and checked against its SHA-256: git's gitk and
# git gui. Its Aqua build (macosx/: AppKit, with the part of Carbon's
# HIToolbox it draws with, which Minkar's Carbon.framework provides), on
# Tcl 8.6 (../tcl8.6), as /System/Library/Frameworks/Tk.framework version
# 8.6, wish8.6 inside it with /usr/bin/wish8.6 and /usr/bin/wish linking to
# it. The SDK gets the framework whole (headers, tkConfig.sh).
#
# Built as ../tcl builds Tcl: configure picks its Darwin build by asking
# uname, which answers as Minkar does; nothing of it runs on the build host.
# Its Aqua build is chosen when Cocoa.framework is there, which configure
# looks for on the build host: a copy of it looks in the SDK. Tk uses
# QuickDraw's Point and Rect (MacTypes.h, QUICKDRAW_TYPES). Its link
# runs nm on Mach-O objects (ours, ahead in PATH), and gives -sectcreate
# to the compiler driver (handed to the linker instead). wish links Tcl
# 8.6 by its path: -framework Tcl would take Current, 8.5.

all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE =		${ROOT_BINARY_DIR}/src-cache
JOBS =		-j${.MAKE.JOBS:U1}
GMAKE =		MAKEFLAGS="" ${TOOLS}/gmake
FW =		System/Library/Frameworks/Tk.framework
TCLFW =		${MINKAR_SDKROOT}/System/Library/Frameworks/Tcl.framework/Versions/8.6
V =		8.6
STAGE =		${.OBJDIR}/stage

SDK_CFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Qunused-arguments -Wno-nullability-completeness
SDK_LDFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		--ld-path=${LD}

# The fetched tree, copied with Minkar's patches (patches/) applied.
PATCHES !=	ls ${.CURDIR}/patches/*.patch 2>/dev/null || true

${STAGE}/${FW}/Versions/${V}/Tk: ${.CURDIR}/MANIFEST ${.CURDIR}/Makefile ${TCLFW}/tclConfig.sh ${PATCHES}
	fetched=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${.OBJDIR}/build ${.OBJDIR}/src ${STAGE} ${.OBJDIR}/fakebin && \
	cp -R $$fetched ${.OBJDIR}/src && \
	for p in ${PATCHES}; do patch -s -d ${.OBJDIR}/src -p1 < $$p || exit 1; done && \
	src=${.OBJDIR}/src && \
	mkdir -p ${.OBJDIR}/build ${.OBJDIR}/fakebin && \
	printf '#!/bin/sh\ncase "$$1" in -r) echo 19.6.0 ;; -m|-p) echo x86_64 ;; -n) echo minkar ;; *) echo Darwin ;; esac\n' \
	    > ${.OBJDIR}/fakebin/uname && chmod 755 ${.OBJDIR}/fakebin/uname && \
	ln -sf ${NM} ${.OBJDIR}/fakebin/nm && \
	cd ${.OBJDIR}/build && export PATH=${.OBJDIR}/fakebin:$$PATH && \
	sed 's|test ! -d /System/Library/Frameworks/Cocoa.framework|test ! -d ${MINKAR_SDKROOT}/System/Library/Frameworks/Cocoa.framework|' \
	    $$src/unix/configure > configure && chmod 755 configure && \
	CC="${CC}" AR=${AR} RANLIB=${RANLIB} NM=${NM:Unm} \
	CFLAGS="${SDK_CFLAGS} -O2 -DQUICKDRAW_TYPES=1" LDFLAGS="${SDK_LDFLAGS}" \
	./configure --srcdir=$$src/unix --prefix=/usr --mandir=/usr/share/man \
	    --libdir=/System/Library/Frameworks --enable-framework --enable-threads \
	    --enable-aqua --with-tcl=${TCLFW} \
	    --build=x86_64-pc-linux-gnu --host=${CpuArch}-apple-darwin19 \
	    > configure.log 2>&1 && \
	sed -i -e 's/-sectcreate \([^ ]*\) \([^ ]*\) \([^ ]*\)/-Wl,-sectcreate,\1,\2,\3/g' Makefile && \
	${GMAKE} ${JOBS} TCL_LIB_SPEC=${TCLFW}/Tcl > make.log 2>&1 && \
	${GMAKE} install-binaries install-libraries install-private-headers \
	    TCL_LIB_SPEC=${TCLFW}/Tcl INSTALL_ROOT=${STAGE} > install.log 2>&1 || \
	    { tail -n 30 ${.OBJDIR}/build/*.log; exit 1; }
	touch ${.TARGET}

# The image: the framework without what only builds use; the SDK: all of it.
${.OBJDIR}/.installed: ${STAGE}/${FW}/Versions/${V}/Tk
	mkdir -p ${SYSROOT_DIR}/${FW} ${MINKAR_SDKROOT}/${FW} ${SYSROOT_DIR}/usr/bin
	rsync -rlc --delete --exclude Headers --exclude PrivateHeaders \
	    --exclude tkConfig.sh --exclude 'libtkstub*' --exclude pkgconfig \
	    ${STAGE}/${FW}/ ${SYSROOT_DIR}/${FW}/
	rsync -rlc --delete ${STAGE}/${FW}/ ${MINKAR_SDKROOT}/${FW}/
	rsync -rl ${STAGE}/usr/bin/ ${SYSROOT_DIR}/usr/bin/
	ln -sf wish${V} ${SYSROOT_DIR}/usr/bin/wish
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/tk && \
	install -C -m 0444 $$src/license.terms ${SYSROOT_DIR}/usr/share/licenses/tk/
	touch ${.TARGET}
