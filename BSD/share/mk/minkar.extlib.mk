# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# External libraries: a project fetched at a pinned version, checked against
# its SHA-256, patched, built as dynamic libraries with its own build system
# (Meson, CMake or autotools) for Minkar, and installed in the SDK (headers,
# libraries, pkg-config files) and in the sysroot (the libraries).
#
# Variables:
#   EXTLIB              the project's name (required)
#   EXTLIB_MANIFEST     url, sha256 and version (default ${.CURDIR}/MANIFEST;
#                       see BSD/lib/libdrm/fetch.sh)
#   EXTLIB_PATCHES      patches in ${.CURDIR}/patches, applied in order
#   EXTLIB_SYSTEM       meson, cmake or autotools (required)
#   EXTLIB_OPTIONS      options for its configure step
#   EXTLIB_TARGETS      what to build (default: everything)
#   EXTLIB_LIBS         the dylibs to install, as found in the staging
#                       prefix's lib directory (e.g. libpcre2-8.0.dylib);
#                       their links (libpcre2-8.dylib) come along
#   EXTLIB_MODULES      loadable modules to install, relative to the staging
#                       prefix (lib/gio/modules/libgioopenssl.so), at the
#                       same place in the sysroot
#   EXTLIB_LICENSES     license files in the source tree, installed in
#                       /usr/share/licenses/${EXTLIB}
#   EXTLIB_SUBDIR       the source tree's subdirectory to build, if not the top
#   EXTLIB_ENV          more environment for every step
#   EXTLIB_SUBPROJECTS  Meson subprojects to put in the tree, each fetched by
#                       ${.CURDIR}/MANIFEST.<name> (Meson itself downloads
#                       nothing: --wrap-mode=nodownload)
#   EXTLIB_HOST         yes: built for the build host instead, into
#                       ${HOST_PREFIX} (the tools other libraries' builds
#                       run, as GLib's), with nothing in the SDK or sysroot
#   EXTLIB_STAGE_REMOVE files the project installs that are not published,
#                       relative to the staging prefix (e.g. include/libintl.h)
#
# The libraries are found by pkg-config in the SDK only, with the SDK as
# sysroot, so one external library can use another installed before it
# (SUBDIR_DEPEND in the parent Makefile).

.if !defined(EXTLIB) || !defined(EXTLIB_SYSTEM)
.error minkar.extlib.mk: EXTLIB and EXTLIB_SYSTEM are required
.endif

EXTLIB_MANIFEST ?=	${.CURDIR}/MANIFEST
EXTLIB_PATCHES ?=
EXTLIB_OPTIONS ?=
EXTLIB_TARGETS ?=
EXTLIB_LIBS ?=
EXTLIB_MODULES ?=
EXTLIB_LICENSES ?=
EXTLIB_ENV ?=
EXTLIB_STAGE_REMOVE ?=
EXTLIB_SUBPROJECTS ?=

MK_DIR :=		${.PARSEDIR}
FETCH =			sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
EXTLIB_CACHE ?=		${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =			${TOOLCHAIN}/usr/bin
SDK =			${MINKAR_SDKROOT}
PYTHON3 ?=		python3

SRC =			${.OBJDIR}/${EXTLIB}-src
BUILD =			${.OBJDIR}/build
STAGE =			${.OBJDIR}/stage
CROSS =			${.OBJDIR}/minkar-cross.ini
CMAKE_TC =		${.OBJDIR}/minkar-toolchain.cmake
LDWRAP =		${.OBJDIR}/ld64-meson
# What external libraries built for the build host install: their tools
# are on every external library's PATH, and named to Meson.
HOST_PREFIX =		${ROOT_BINARY_DIR:U${.OBJDIR}}/extlib-host
HOST_TOOLS =		glib-mkenums glib-genmarshal glib-compile-resources \
			glib-compile-schemas gdbus-codegen
HOST_INI =		${.OBJDIR}/host-tools.ini
# Meson, shared by every external library (the pinned one Mesa uses).
VENV =			${ROOT_BINARY_DIR:U${.OBJDIR}}/extlib-venv
REQUIREMENTS =		${ROOT_SOURCE_DIR}/BSD/lib/mesa/requirements.txt
PIP_WHEEL =		pip-25.0.1-py3-none-any.whl
PIP_URL =		https://files.pythonhosted.org/packages/c9/bc/b7db44f5f39f9d0494071bddae6880eb645970366d0a200022a1a93d57f5/${PIP_WHEEL}
PIP_SHA256 =		c46efd13b6aa8279f33f2864459c8ce587ea6a1a59ee20de055868d8f7688f7f

TARGET_FLAGS =		-target x86_64-apple-macosx10.15 --sysroot=${SDK} -mmacos-version-min=10.15
WARN_FLAGS =		-Wno-nullability-completeness -Wno-incompatible-sysroot

# The toolchain's clang refuses more than one *_DEPLOYMENT_TARGET in the
# environment; the flags set the target already. pkg-config sees the SDK
# only.
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET -u IPHONEOS_DEPLOYMENT_TARGET \
		-u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET \
		-u XROS_DEPLOYMENT_TARGET -u DRIVERKIT_DEPLOYMENT_TARGET \
		PATH=${VENV}/bin:${HOST_PREFIX}/bin:$${PATH} \
		PKG_CONFIG_LIBDIR=${SDK}/usr/lib/pkgconfig:${SDK}/usr/local/lib/pkgconfig PKG_CONFIG_PATH= \
		PKG_CONFIG_SYSROOT_DIR=${SDK} ${EXTLIB_ENV}
.if defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes"
# For the build host: its compilers, and the libraries built for it before.
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET PATH=${VENV}/bin:${HOST_PREFIX}/bin:$${PATH} \
		PKG_CONFIG_PATH=${HOST_PREFIX}/lib/pkgconfig \
		LDFLAGS="-Wl,-rpath,${HOST_PREFIX}/lib" ${EXTLIB_ENV}
.endif

all: install-extlib

# The tree with our patches, made again when they or the manifest change.
${SRC}/.minkar-patches: ${EXTLIB_PATCHES:@p@${.CURDIR}/patches/${p}@} ${EXTLIB_MANIFEST} \
    ${EXTLIB_SUBPROJECTS:@p@${.CURDIR}/MANIFEST.${p}@}
	pristine=$$(${FETCH} ${EXTLIB_MANIFEST} ${EXTLIB_CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part && \
	for sp in ${EXTLIB_SUBPROJECTS}; do \
		dir=$$(${FETCH} ${.CURDIR}/MANIFEST.$$sp ${EXTLIB_CACHE}) || exit 1; \
		rm -rf ${SRC}.part/subprojects/$$sp && cp -a $$dir ${SRC}.part/subprojects/$$sp || exit 1; \
	done && \
	for p in ${EXTLIB_PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The build files for each system: Meson's cross file (with its linker
# wrapper), a CMake toolchain file, or the environment autotools takes.
${CROSS}: ${ROOT_SOURCE_DIR}/BSD/lib/mesa/minkar-cross.ini.in ${ROOT_SOURCE_DIR}/BSD/lib/mesa/ld64-meson.in
	sed -e 's|@TC@|${TC}|g' ${ROOT_SOURCE_DIR}/BSD/lib/mesa/ld64-meson.in > ${LDWRAP}
	chmod 0755 ${LDWRAP}
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${SDK}|g' -e 's|@LD@|${LDWRAP}|g' \
	    ${ROOT_SOURCE_DIR}/BSD/lib/mesa/minkar-cross.ini.in > ${.TARGET}
	echo "sys_root = '${SDK}'" >> ${.TARGET}
	sed -i "s|^pkg_config_libdir = .*|pkg_config_libdir = ['${SDK}/usr/lib/pkgconfig', '${SDK}/usr/local/lib/pkgconfig']|" ${.TARGET}

# The build host's tools, as a second cross file: written when the
# Makefile is read, and only when it changes (Meson is set up again then).
.if !(defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes")
_HOST_INI != mkdir -p ${.OBJDIR} && { echo '[binaries]'; \
	for t in ${HOST_TOOLS}; do \
		[ -x ${HOST_PREFIX}/bin/$$t ] && echo "$$t = '${HOST_PREFIX}/bin/$$t'"; \
	done; true; } > ${HOST_INI}.new && \
	{ cmp -s ${HOST_INI}.new ${HOST_INI} || mv ${HOST_INI}.new ${HOST_INI}; } && \
	rm -f ${HOST_INI}.new; echo
.endif

${CMAKE_TC}: ${MK_DIR}/minkar.extlib.mk
	{ echo 'set(CMAKE_SYSTEM_NAME Darwin)'; \
	  echo 'set(CMAKE_SYSTEM_PROCESSOR x86_64)'; \
	  echo 'set(CMAKE_C_COMPILER ${TC}/clang)'; \
	  echo 'set(CMAKE_CXX_COMPILER ${TC}/clang++)'; \
	  echo 'set(CMAKE_AR ${TC}/llvm-ar)'; \
	  echo 'set(CMAKE_RANLIB ${TC}/llvm-ranlib)'; \
	  echo 'set(CMAKE_INSTALL_NAME_TOOL ${TC}/install_name_tool)'; \
	  echo 'set(CMAKE_LINKER ${TC}/ld64)'; \
	  echo 'set(CMAKE_OSX_SYSROOT ${SDK})'; \
	  echo 'set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)'; \
	  echo 'set(CMAKE_OSX_ARCHITECTURES x86_64)'; \
	  echo 'set(CMAKE_C_FLAGS_INIT "${TARGET_FLAGS} ${WARN_FLAGS}")'; \
	  echo 'set(CMAKE_CXX_FLAGS_INIT "${TARGET_FLAGS} ${WARN_FLAGS}")'; \
	  echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "${TARGET_FLAGS} -fuse-ld=${TC}/ld64")'; \
	  echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "${TARGET_FLAGS} -fuse-ld=${TC}/ld64")'; \
	  echo 'set(CMAKE_FIND_ROOT_PATH ${SDK})'; \
	  echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)'; \
	  echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)'; \
	  echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)'; \
	  echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)'; \
	} > ${.TARGET}

AUTOTOOLS_ENV =	CC="${TC}/clang ${TARGET_FLAGS}" CXX="${TC}/clang++ ${TARGET_FLAGS}" \
		CFLAGS="-O2 ${WARN_FLAGS}" CXXFLAGS="-O2 ${WARN_FLAGS}" \
		LDFLAGS="-fuse-ld=${TC}/ld64" AR=${TC}/llvm-ar RANLIB=${TC}/llvm-ranlib \
		NM=${TC}/llvm-nm STRIP=${TC}/llvm-strip LD=${TC}/ld64 \
		OTOOL=${TC}/otool OTOOL64=${TC}/otool lt_cv_prog_gnu_ld=no

# Meson, pinned, in a venv shared by the external libraries.
${VENV}/.minkar-requirements: ${REQUIREMENTS}
	mkdir -p ${EXTLIB_CACHE}
	test -f ${EXTLIB_CACHE}/${PIP_WHEEL} || \
	    curl -sfL -o ${EXTLIB_CACHE}/${PIP_WHEEL} ${PIP_URL}
	echo "${PIP_SHA256}  ${EXTLIB_CACHE}/${PIP_WHEEL}" | sha256sum -c --quiet
	rm -rf ${VENV} && ${PYTHON3} -m venv --without-pip ${VENV}
	${VENV}/bin/python ${EXTLIB_CACHE}/${PIP_WHEEL}/pip install -q --disable-pip-version-check \
	    --require-hashes -r ${REQUIREMENTS}
	cksum < ${REQUIREMENTS} > ${.TARGET}

EXTLIB_TOP =	${SRC}${EXTLIB_SUBDIR:D/${EXTLIB_SUBDIR}}

# Configure again when the tree, the build files or the Makefile change.
.if defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes"
.if ${EXTLIB_SYSTEM} == "meson"
SETUP_DEPS =	${VENV}/.minkar-requirements
SETUP_CMD =	meson setup ${BUILD} ${EXTLIB_TOP} --wrap-mode=nodownload -Dbuildtype=release \
		-Dprefix=${HOST_PREFIX} -Dlibdir=lib ${EXTLIB_OPTIONS}
.elif ${EXTLIB_SYSTEM} == "cmake"
SETUP_DEPS =
SETUP_CMD =	cmake -G Ninja -S ${EXTLIB_TOP} -B ${BUILD} -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=${HOST_PREFIX} -DCMAKE_INSTALL_LIBDIR=lib \
		-DCMAKE_INSTALL_RPATH=${HOST_PREFIX}/lib ${EXTLIB_OPTIONS}
.else
.error minkar.extlib.mk: EXTLIB_HOST builds with meson or cmake
.endif
BUILD_CMD =	ninja -C ${BUILD} ${EXTLIB_TARGETS}
.elif ${EXTLIB_SYSTEM} == "meson"
SETUP_DEPS =	${CROSS} ${HOST_INI} ${VENV}/.minkar-requirements
SETUP_CMD =	meson setup ${BUILD} ${EXTLIB_TOP} --cross-file=${CROSS} --cross-file=${HOST_INI} \
		--wrap-mode=nodownload \
		-Dbuildtype=release -Dprefix=/usr -Dlibdir=lib -Ddefault_library=shared ${EXTLIB_OPTIONS}
BUILD_CMD =	ninja -C ${BUILD} ${EXTLIB_TARGETS}
STAGE_CMD =	DESTDIR=${STAGE} ninja -C ${BUILD} install
.elif ${EXTLIB_SYSTEM} == "cmake"
SETUP_DEPS =	${CMAKE_TC}
SETUP_CMD =	cmake -G Ninja -S ${EXTLIB_TOP} -B ${BUILD} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TC} \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
		-DCMAKE_INSTALL_NAME_DIR=/usr/lib -DBUILD_SHARED_LIBS=ON ${EXTLIB_OPTIONS}
BUILD_CMD =	ninja -C ${BUILD} ${EXTLIB_TARGETS}
STAGE_CMD =	DESTDIR=${STAGE} ninja -C ${BUILD} install
.elif ${EXTLIB_SYSTEM} == "autotools"
SETUP_DEPS =
SETUP_CMD =	mkdir -p ${BUILD} && cd ${BUILD} && ${AUTOTOOLS_ENV} ${EXTLIB_TOP}/configure \
		--host=x86_64-apple-darwin19 --build=$$(sh ${EXTLIB_TOP}/config.guess) \
		--prefix=/usr --libdir=/usr/lib --enable-shared --disable-static ${EXTLIB_OPTIONS}
BUILD_CMD =	gmake -C ${BUILD} ${EXTLIB_TARGETS}
STAGE_CMD =	gmake -C ${BUILD} DESTDIR=${STAGE} install
.else
.error minkar.extlib.mk: EXTLIB_SYSTEM ${EXTLIB_SYSTEM}: meson, cmake or autotools
.endif
.if ${EXTLIB_SYSTEM} == "autotools" && !(defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes")
BUILD_CMD =	gmake -C ${BUILD} -j${.MAKE.JOBS:U1} ${EXTLIB_TARGETS}
.endif

${BUILD}/.minkar-setup: ${SRC}/.minkar-patches ${SETUP_DEPS} ${.CURDIR}/Makefile ${MK_DIR}/minkar.extlib.mk
	rm -rf ${BUILD}
	${TOOLENV} sh -c '${SETUP_CMD}' > ${.OBJDIR}/setup.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/setup.log; exit 1; }
	touch ${.TARGET}

build-extlib: .PHONY ${BUILD}/.minkar-setup
	${TOOLENV} sh -c '${BUILD_CMD}'

.if defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes"
install-extlib: .PHONY build-extlib
	mkdir -p ${HOST_PREFIX}
	${TOOLENV} ninja -C ${BUILD} install > ${.OBJDIR}/stage.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/stage.log; exit 1; }
.else
# Staged, then copied: headers, libraries and pkg-config files to the SDK
# (pkg-config's sysroot makes their /usr paths the SDK's), the libraries
# to the sysroot. Each library's install name is /usr/lib/<its name>.
install-extlib: .PHONY build-extlib
	rm -rf ${STAGE}
	${TOOLENV} sh -c '${STAGE_CMD}' > ${.OBJDIR}/stage.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/stage.log; exit 1; }
.for f in ${EXTLIB_STAGE_REMOVE}
	rm -f ${STAGE}/usr/${f}
.endfor
	mkdir -p ${SDK}/usr/include ${SDK}/usr/lib/pkgconfig ${SYSROOT_DIR}/usr/lib
	if [ -d ${STAGE}/usr/include ]; then cp -R ${STAGE}/usr/include/. ${SDK}/usr/include/; fi
	if [ -d ${STAGE}/usr/lib/pkgconfig ]; then cp -R ${STAGE}/usr/lib/pkgconfig/. ${SDK}/usr/lib/pkgconfig/; fi
	# Other directories of lib (configuration headers: glib-2.0/include).
	for d in ${STAGE}/usr/lib/*/; do \
		case $$d in */pkgconfig/) ;; *) [ -d "$$d" ] && cp -R "$$d" ${SDK}/usr/lib/ ;; esac; \
	done
	cd ${STAGE}/usr/lib && for lib in ${EXTLIB_LIBS}; do \
		test -f $$lib || { echo "minkar.extlib.mk: $$lib not built" >&2; exit 1; }; \
		${TC}/install_name_tool -id /usr/lib/$$lib $$lib 2>/dev/null; \
		for dep in $$(${TC}/otool -L $$lib | awk '$$1 ~ /^@rpath\// { print $$1 }'); do \
			${TC}/install_name_tool -change $$dep /usr/lib/$${dep#@rpath/} $$lib || exit 1; \
		done; \
		base=$${lib%%.*}; \
		for f in $$base.*dylib; do \
			cp -P $$f ${SDK}/usr/lib/ && cp -P $$f ${SYSROOT_DIR}/usr/lib/ || exit 1; \
		done; \
	done
	for m in ${EXTLIB_MODULES}; do \
		f=${STAGE}/usr/$$m; test -f $$f || { echo "minkar.extlib.mk: $$m not built" >&2; exit 1; }; \
		for dep in $$(${TC}/otool -L $$f | awk '$$1 ~ /^@rpath\// { print $$1 }'); do \
			${TC}/install_name_tool -change $$dep /usr/lib/$${dep#@rpath/} $$f || exit 1; \
		done; \
		mkdir -p ${SYSROOT_DIR}/usr/$${m%/*} && cp -f $$f ${SYSROOT_DIR}/usr/$$m || exit 1; \
	done
.if !empty(EXTLIB_LICENSES)
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/${EXTLIB}
	cd ${SRC} && install -m 0444 ${EXTLIB_LICENSES} ${SYSROOT_DIR}/usr/share/licenses/${EXTLIB}/
.endif
.endif

clean: .PHONY
	rm -rf ${SRC} ${BUILD} ${STAGE} ${CROSS} ${LDWRAP} ${CMAKE_TC} ${.OBJDIR}/setup.log ${.OBJDIR}/stage.log

.include <bsd.prog.mk>
