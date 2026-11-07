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
#   EXTLIB_SOURCE       a source tree already in the repository, instead of
#                       a fetched one (no manifest)
#   EXTLIB_PATCHES      patches in ${EXTLIB_DIR}/patches, applied in order
#   EXTLIB_SYSTEM       meson, cmake or autotools (required)
#   EXTLIB_OPTIONS      options for its configure step
#   EXTLIB_TARGETS      what to build (default: everything)
#   EXTLIB_STAGE_CMD    how to install into ${STAGE} (default: the whole
#                       project's install)
#   EXTLIB_LIBS         the dylibs to install, as found in the staging
#                       prefix's lib directory (e.g. libpcre2-8.0.dylib);
#                       their links (libpcre2-8.dylib) come along
#   EXTLIB_PUBLIC       yes: a library the reference platform has in
#                       /usr/lib too (libxml2, libxslt, Expat), installed
#                       there with its headers and pkg-config file in the
#                       SDK's usr. Otherwise it is WebKit's alone: in
#                       /usr/lib/webkit, its headers and pkg-config file in
#                       the SDK's usr/local/webkit, out of the public API
#   EXTLIB_MODULES      loadable modules to install, relative to the staging
#                       prefix (lib/gio/modules/libgioopenssl.so), at the
#                       same place in the sysroot
#   EXTLIB_SDK_FILES    files for the SDK only, relative to the staging root
#                       (libgpg-error's usr/bin/gpgrt-config, which other
#                       libraries' configure scripts run)
#   EXTLIB_DATA         data the project installs, relative to the staging
#                       root (usr/share/X11/xkb), copied to the sysroot
#   EXTLIB_LICENSES     license files in the source tree, installed in
#                       /usr/share/licenses/${EXTLIB}
#   EXTLIB_SUBDIR       the source tree's subdirectory to build, if not the top
#   EXTLIB_FRAMEWORKS   frameworks the project installs in lib (names without
#                       .framework), copied to LIB_DEST with their install
#                       names there
#   EXTLIB_HELPERS      programs of the build directory put in a framework,
#                       as PATH:FRAMEWORK (e.g. bin/jsc:JavaScriptCore): in
#                       its Versions/Current/Helpers, using it from LIB_DEST
#   EXTLIB_ENV          more environment for every step
#   EXTLIB_SUBPROJECTS  Meson subprojects to put in the tree, each fetched by
#                       ${EXTLIB_DIR}/MANIFEST.<name> (Meson itself downloads
#                       nothing: --wrap-mode=nodownload)
#   EXTLIB_SUBPROJECTS_DIR where in the tree they go (default subprojects;
#                       e.g. srclib, for the libraries httpd builds with it)
#   EXTLIB_HOST         yes: built for the build host instead, into
#                       ${HOST_PREFIX} (the tools other libraries' builds
#                       run, as GLib's), with nothing in the SDK or sysroot
#   EXTLIB_TEST         yes: built again with its own tests (gate W0, in
#                       Tests/W0), which go to EXTLIB_TEST_PATH (default
#                       /usr/local/tests/w0/${EXTLIB}) with ${.CURDIR}/run.sh
#                       if there is one; the libraries they use are
#                       the system's (their @rpath references become
#                       /usr/lib), nothing else is installed
#   EXTLIB_TEST_BUILD   a command that builds the tests after the library
#                       (autotools: gmake -C ${BUILD}/tests check TESTS=,
#                       which builds them without running them)
#   EXTLIB_TEST_FILES   what to install for the tests: paths in the staging
#                       root or, with build/ or src/ before them, in the
#                       build or source directory (a directory is copied
#                       whole)
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
EXTLIB_DATA ?=
EXTLIB_SDK_FILES ?=
EXTLIB_LICENSES ?=
EXTLIB_ENV ?=
EXTLIB_STAGE_REMOVE ?=
EXTLIB_SUBPROJECTS ?=
EXTLIB_SUBPROJECTS_DIR ?= subprojects
# Where patches/ and the subprojects' manifests are: the library's own
# directory (a test build in Tests/W0 names it).
EXTLIB_DIR ?=		${.CURDIR}

# Where a library goes: /usr/lib and the SDK's usr (public), or WebKit's
# private /usr/lib/webkit and usr/local/webkit.
PRIVATE_LIB =		/usr/lib/webkit
PRIVATE_DEV =		/usr/local/webkit
.if defined(EXTLIB_PUBLIC) && ${EXTLIB_PUBLIC} == "yes"
LIB_DEST =		/usr/lib
DEV_DEST =		/usr
.else
LIB_DEST =		${PRIVATE_LIB}
DEV_DEST =		${PRIVATE_DEV}
.endif

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
			glib-compile-schemas gdbus-codegen gio-querymodules
HOST_INI =		${.OBJDIR}/host-tools.ini
# Meson, shared by every external library (the pinned one Mesa uses).
VENV =			${ROOT_BINARY_DIR:U${.OBJDIR}}/extlib-venv
REQUIREMENTS =		${ROOT_SOURCE_DIR}/BSD/lib/mesa/requirements.txt \
			${ROOT_SOURCE_DIR}/BSD/lib/extlib-requirements.txt
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
		PKG_CONFIG_LIBDIR=${SDK}/usr/lib/pkgconfig:${SDK}/usr/local/lib/pkgconfig:${SDK}${PRIVATE_DEV}/lib/pkgconfig \
		PKG_CONFIG_PATH= \
		PKG_CONFIG_SYSROOT_DIR=${SDK} ${EXTLIB_ENV}
.if defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes"
# For the build host: its compilers, and the libraries built for it before.
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET PATH=${VENV}/bin:${HOST_PREFIX}/bin:$${PATH} \
		PKG_CONFIG_PATH=${HOST_PREFIX}/lib/pkgconfig \
		LDFLAGS="-Wl,-rpath,${HOST_PREFIX}/lib" ${EXTLIB_ENV}
.endif

all: install-extlib

# The tree with our patches, made again when they or the manifest change.
.if defined(EXTLIB_SOURCE)
_EXTLIB_ORIGIN =	${.CURDIR}/Makefile
_EXTLIB_PRISTINE =	echo ${EXTLIB_SOURCE}
.else
_EXTLIB_ORIGIN =	${EXTLIB_MANIFEST}
_EXTLIB_PRISTINE =	${FETCH} ${EXTLIB_MANIFEST} ${EXTLIB_CACHE}
.endif
${SRC}/.minkar-patches: ${EXTLIB_PATCHES:@p@${EXTLIB_DIR}/patches/${p}@} ${_EXTLIB_ORIGIN} \
    ${EXTLIB_SUBPROJECTS:@p@${EXTLIB_DIR}/MANIFEST.${p}@}
	pristine=$$(${_EXTLIB_PRISTINE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part && \
	for sp in ${EXTLIB_SUBPROJECTS}; do \
		dir=$$(${FETCH} ${EXTLIB_DIR}/MANIFEST.$$sp ${EXTLIB_CACHE}) || exit 1; \
		mkdir -p ${SRC}.part/${EXTLIB_SUBPROJECTS_DIR} && rm -rf ${SRC}.part/${EXTLIB_SUBPROJECTS_DIR}/$$sp && \
		    cp -a $$dir ${SRC}.part/${EXTLIB_SUBPROJECTS_DIR}/$$sp || exit 1; \
	done && \
	for p in ${EXTLIB_PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${EXTLIB_DIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The build files for each system: Meson's cross file (with its linker
# wrapper), a CMake toolchain file, or the environment autotools takes.
${CROSS}: ${ROOT_SOURCE_DIR}/BSD/lib/mesa/minkar-cross.ini.in ${ROOT_SOURCE_DIR}/BSD/lib/mesa/ld64-meson.in \
    ${MK_DIR}/minkar.extlib.mk
	sed -e 's|@TC@|${TC}|g' ${ROOT_SOURCE_DIR}/BSD/lib/mesa/ld64-meson.in > ${LDWRAP}
	chmod 0755 ${LDWRAP}
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${SDK}|g' -e 's|@LD@|${LDWRAP}|g' \
	    ${ROOT_SOURCE_DIR}/BSD/lib/mesa/minkar-cross.ini.in > ${.TARGET}
	echo "sys_root = '${SDK}'" >> ${.TARGET}
	sed -i "s|^pkg_config_libdir = .*|pkg_config_libdir = ['${SDK}/usr/lib/pkgconfig', '${SDK}/usr/local/lib/pkgconfig', '${SDK}${PRIVATE_DEV}/lib/pkgconfig']|" ${.TARGET}

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

# The linker in CC itself (libtool drops LDFLAGS when it links a library),
# unused when CC only compiles. Libtool links a library with undefined
# symbols allowed unless the deployment target says a recent system.
AUTOTOOLS_ENV =	CC="${TC}/clang ${TARGET_FLAGS} -fuse-ld=${TC}/ld64 -Qunused-arguments" \
		CXX="${TC}/clang++ ${TARGET_FLAGS} -fuse-ld=${TC}/ld64 -Qunused-arguments" \
		CFLAGS="-O2 ${WARN_FLAGS}" CXXFLAGS="-O2 ${WARN_FLAGS}" \
		LDFLAGS="-fuse-ld=${TC}/ld64" AR=${TC}/llvm-ar RANLIB=${TC}/llvm-ranlib \
		NM=${TC}/llvm-nm STRIP=${TC}/llvm-strip LD=${TC}/ld64 \
		OTOOL=${TC}/otool OTOOL64=${TC}/otool lt_cv_prog_gnu_ld=no \
		MACOSX_DEPLOYMENT_TARGET=10.15

# Meson, pinned, in a venv shared by the external libraries.
${VENV}/.minkar-requirements: ${REQUIREMENTS}
	mkdir -p ${EXTLIB_CACHE}
	test -f ${EXTLIB_CACHE}/${PIP_WHEEL} || \
	    curl -sfL -o ${EXTLIB_CACHE}/${PIP_WHEEL} ${PIP_URL}
	echo "${PIP_SHA256}  ${EXTLIB_CACHE}/${PIP_WHEEL}" | sha256sum -c --quiet
	rm -rf ${VENV} && ${PYTHON3} -m venv --without-pip ${VENV}
	${VENV}/bin/python ${EXTLIB_CACHE}/${PIP_WHEEL}/pip install -q --disable-pip-version-check \
	    --require-hashes ${REQUIREMENTS:@r@-r ${r}@}
	cat ${REQUIREMENTS} | cksum > ${.TARGET}

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
		-DCMAKE_INSTALL_NAME_DIR=${LIB_DEST} -DBUILD_SHARED_LIBS=ON ${EXTLIB_OPTIONS}
BUILD_CMD =	ninja -C ${BUILD} ${EXTLIB_TARGETS}
STAGE_CMD =	DESTDIR=${STAGE} ninja -C ${BUILD} install
.elif ${EXTLIB_SYSTEM} == "autotools"
SETUP_DEPS =
SETUP_CMD =	mkdir -p ${BUILD} && cd ${BUILD} && ${AUTOTOOLS_ENV} ${EXTLIB_TOP}/configure \
		--host=x86_64-apple-darwin19 \
		--build=$$(sh $$(ls ${EXTLIB_TOP}/config.guess ${EXTLIB_TOP}/build-aux/config.guess 2>/dev/null | head -1)) \
		--prefix=/usr --libdir=/usr/lib --enable-shared --disable-static ${EXTLIB_OPTIONS}
BUILD_CMD =	gmake -C ${BUILD} ${EXTLIB_TARGETS}
STAGE_CMD =	gmake -C ${BUILD} DESTDIR=${STAGE} install
.else
.error minkar.extlib.mk: EXTLIB_SYSTEM ${EXTLIB_SYSTEM}: meson, cmake or autotools
.endif
.if ${EXTLIB_SYSTEM} == "autotools" && !(defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes")
BUILD_CMD =	gmake -C ${BUILD} -j${.MAKE.JOBS:U1} ${EXTLIB_TARGETS}
.endif

.if defined(EXTLIB_STAGE_CMD)
STAGE_CMD =	${EXTLIB_STAGE_CMD}
.endif

${BUILD}/.minkar-setup: ${SRC}/.minkar-patches ${SETUP_DEPS} ${.CURDIR}/Makefile ${MK_DIR}/minkar.extlib.mk
	rm -rf ${BUILD}
	${TOOLENV} sh -c '${SETUP_CMD}' > ${.OBJDIR}/setup.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/setup.log; exit 1; }
	touch ${.TARGET}

build-extlib: .PHONY ${BUILD}/.minkar-setup
	${TOOLENV} sh -c '${BUILD_CMD}'
.if defined(EXTLIB_TEST_BUILD)
	${TOOLENV} ${AUTOTOOLS_ENV} sh -c '${EXTLIB_TEST_BUILD}'
.endif

.if defined(EXTLIB_TEST) && ${EXTLIB_TEST} == "yes"
EXTLIB_TEST_PATH ?= /usr/local/tests/w0/${EXTLIB}
TEST_DIR =	${SYSROOT_DIR}${EXTLIB_TEST_PATH}
install-extlib: .PHONY build-extlib
	rm -rf ${STAGE} ${TEST_DIR}
	${TOOLENV} sh -c '${STAGE_CMD}' > ${.OBJDIR}/stage.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/stage.log; exit 1; }
	mkdir -p ${TEST_DIR}
	for f in ${EXTLIB_TEST_FILES}; do \
		case $$f in build/*) src=${BUILD}/$${f#build/};; src/*) src=${SRC}/$${f#src/};; \
		    *) src=${STAGE}/$$f;; esac; \
		test -e $$src || { echo "minkar.extlib.mk: $$f not built" >&2; exit 1; }; \
		cp -RL $$src ${TEST_DIR}/ || exit 1; \
	done
	# The system's libraries, not the test build's own: a library of the
	# build (@rpath, or a path in the build) is /usr/lib's when the system
	# has it, else a copy beside the tests.
	find ${TEST_DIR} -type f | while read -r f; do \
		${TC}/otool -L "$$f" 2>/dev/null | \
		    awk 'NR > 1 && ($$1 ~ /^@rpath\// || $$1 ~ /^\/usr\/lib\/[^\/]*$$/ || \
		        index($$1, "${.OBJDIR}/") == 1) { print $$1 }' | \
		while read -r dep; do \
			name=$${dep##*/}; \
			case $$dep in /usr/lib/*) [ -e ${SYSROOT_DIR}$$dep ] && continue;; esac; \
			here=$$(find ${TEST_DIR} \( -name $$name -o -name $${name%.dylib}.so \) | head -1); \
			if [ -e ${SYSROOT_DIR}${PRIVATE_LIB}/$$name ]; then new=${PRIVATE_LIB}/$$name; \
			elif [ -e ${SYSROOT_DIR}/usr/lib/$$name ]; then new=/usr/lib/$$name; \
			elif [ -n "$$here" ]; then new=${EXTLIB_TEST_PATH}/$${here#${TEST_DIR}/}; \
			else \
				built=$$(find ${BUILD} -name $$name | head -1); \
				[ -n "$$built" ] || { echo "minkar.extlib.mk: $$name not found" >&2; exit 1; }; \
				cp -L $$built ${TEST_DIR}/ || exit 1; \
				new=${EXTLIB_TEST_PATH}/$$name; \
			fi; \
			${TC}/install_name_tool -change $$dep $$new "$$f" 2>/dev/null; \
		done; \
	done; true
	# Libraries among the tests answer to where they are now.
	find ${TEST_DIR} \( -name '*.dylib' -o -name '*.so' \) -type f | while read -r f; do \
		${TC}/install_name_tool -id ${EXTLIB_TEST_PATH}/$${f#${TEST_DIR}/} "$$f"; \
		${TC}/otool -L "$$f" | awk '$$1 ~ /^@rpath\// || index($$1, "${.OBJDIR}/") == 1 { print $$1 }' | \
		while read -r dep; do \
			n=$${dep##*/}; \
			if [ -e ${TEST_DIR}/$$n ]; then n=${EXTLIB_TEST_PATH}/$$n; \
			elif [ -e ${SYSROOT_DIR}${PRIVATE_LIB}/$$n ]; then n=${PRIVATE_LIB}/$$n; else n=/usr/lib/$$n; fi; \
			${TC}/install_name_tool -change $$dep $$n "$$f"; \
		done; \
	done; true
	if [ -f ${.CURDIR}/run.sh ]; then install -m 0755 ${.CURDIR}/run.sh ${TEST_DIR}/run.sh; fi
	# What is left out, and why (if anything is).
	if [ -f ${.CURDIR}/skip.txt ]; then install -m 0644 ${.CURDIR}/skip.txt ${TEST_DIR}/; fi
.elif defined(EXTLIB_HOST) && ${EXTLIB_HOST} == "yes"
install-extlib: .PHONY build-extlib
	mkdir -p ${HOST_PREFIX}
	${TOOLENV} ninja -C ${BUILD} install > ${.OBJDIR}/stage.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/stage.log; exit 1; }
.else
# Staged, then copied: headers, libraries and pkg-config files to the SDK
# (pkg-config's sysroot makes their paths the SDK's), the libraries to the
# sysroot, at LIB_DEST and DEV_DEST. Each library's install name is
# LIB_DEST/<its name>; a private library's pkg-config file says where it
# is. A reference to another library of the build (@rpath) is to it at
# LIB_DEST.
PC_FIX =	-e 's|^prefix=.*|prefix=${DEV_DEST}|' -e 's|^libdir=.*|libdir=${LIB_DEST}|' \
		-e 's|^includedir=/usr/include|includedir=${DEV_DEST}/include|'
.if ${DEV_DEST} != "/usr"
# Out of the default paths: the -I and -L a file written for /usr leaves out.
PC_FIX +=	-e '/^Cflags:/{/-I/!s|^Cflags:|Cflags: -I$${includedir}|;}' \
		-e '/^Libs:/{/-L/!s|^Libs:|Libs: -L$${libdir}|;}'
.endif
install-extlib: .PHONY build-extlib
	rm -rf ${STAGE}
	${TOOLENV} sh -c '${STAGE_CMD}' > ${.OBJDIR}/stage.log 2>&1 || \
	    { tail -40 ${.OBJDIR}/stage.log; exit 1; }
.for f in ${EXTLIB_STAGE_REMOVE}
	rm -f ${STAGE}/usr/${f}
.endfor
	mkdir -p ${SDK}${DEV_DEST}/include ${SDK}${DEV_DEST}/lib/pkgconfig ${SDK}${LIB_DEST} \
	    ${SYSROOT_DIR}${LIB_DEST}
	if [ -d ${STAGE}/usr/include ]; then cp -R ${STAGE}/usr/include/. ${SDK}${DEV_DEST}/include/; fi
	if [ -d ${STAGE}/usr/lib/pkgconfig ]; then \
		for pc in ${STAGE}/usr/lib/pkgconfig/*.pc; do \
			sed ${PC_FIX} $$pc > ${SDK}${DEV_DEST}/lib/pkgconfig/$${pc##*/} || exit 1; \
		done; \
	fi
	# Other directories of lib (configuration headers: glib-2.0/include).
	for d in ${STAGE}/usr/lib/*/; do \
		case $$d in */pkgconfig/|*/webkit/) ;; *) [ -d "$$d" ] && cp -R "$$d" ${SDK}${LIB_DEST}/ ;; esac; \
	done
	cd ${STAGE}/usr/lib && for lib in ${EXTLIB_LIBS}; do \
		test -f $$lib || { echo "minkar.extlib.mk: $$lib not built" >&2; exit 1; }; \
		${TC}/install_name_tool -id ${LIB_DEST}/$$lib $$lib 2>/dev/null; \
		for dep in $$(${TC}/otool -L $$lib | awk 'NR > 1 { print $$1 }'); do \
			n=$${dep##*/}; \
			case $$dep in @rpath/*|/usr/lib/*) ;; *) continue;; esac; \
			[ -e ${STAGE}/usr/lib/$$n ] && [ $$dep != ${LIB_DEST}/$$n ] || continue; \
			${TC}/install_name_tool -change $$dep ${LIB_DEST}/$$n $$lib || exit 1; \
		done; \
		base=$${lib%%.*}; \
		for f in $$base.*dylib; do \
			cp -P $$f ${SDK}${LIB_DEST}/ && cp -P $$f ${SYSROOT_DIR}${LIB_DEST}/ || exit 1; \
		done; \
	done
	for fw in ${EXTLIB_FRAMEWORKS}; do \
		d=${STAGE}/usr/lib/$$fw.framework; \
		test -d $$d || { echo "minkar.extlib.mk: $$fw.framework not built" >&2; exit 1; }; \
		v=$$(readlink $$d/Versions/Current); \
		${TC}/install_name_tool -id ${LIB_DEST}/$$fw.framework/Versions/$$v/$$fw $$d/Versions/$$v/$$fw || exit 1; \
		for dest in ${SDK}${LIB_DEST} ${SYSROOT_DIR}${LIB_DEST}; do \
			rm -rf $$dest/$$fw.framework && cp -R $$d $$dest/ || exit 1; \
		done; \
	done
	for h in ${EXTLIB_HELPERS}; do \
		prog=${BUILD}/$${h%%:*}; fw=$${h##*:}; \
		test -f $$prog || { echo "minkar.extlib.mk: $${h%%:*} not built" >&2; exit 1; }; \
		v=$$(readlink ${SYSROOT_DIR}${LIB_DEST}/$$fw.framework/Versions/Current); \
		dir=${SYSROOT_DIR}${LIB_DEST}/$$fw.framework/Versions/$$v/Helpers; \
		mkdir -p $$dir && cp -f $$prog $$dir/ || exit 1; \
		old=$$(${TC}/otool -L $$prog | awk -v f="/$$fw.framework/" 'NR > 1 && index($$1, f) { print $$1 }'); \
		[ -z "$$old" ] || ${TC}/install_name_tool -change $$old \
		    ${LIB_DEST}/$$fw.framework/Versions/$$v/$$fw $$dir/$${prog##*/} || exit 1; \
	done
	for m in ${EXTLIB_MODULES}; do \
		f=${STAGE}/usr/$$m; test -f $$f || { echo "minkar.extlib.mk: $$m not built" >&2; exit 1; }; \
		for dep in $$(${TC}/otool -L $$f | awk '$$1 ~ /^@rpath\// { print $$1 }'); do \
			${TC}/install_name_tool -change $$dep ${LIB_DEST}/$${dep#@rpath/} $$f || exit 1; \
		done; \
		mkdir -p ${SYSROOT_DIR}/usr/$${m%/*} && cp -f $$f ${SYSROOT_DIR}/usr/$$m || exit 1; \
	done
	for f in ${EXTLIB_SDK_FILES}; do \
		d=${SDK}${DEV_DEST}/$${f#usr/}; \
		mkdir -p $${d%/*} && cp -f ${STAGE}/$$f $$d || exit 1; \
	done
	for d in ${EXTLIB_DATA}; do \
		test -e ${STAGE}/$$d || { echo "minkar.extlib.mk: $$d not installed" >&2; exit 1; }; \
		rm -rf ${SYSROOT_DIR}/$$d && mkdir -p ${SYSROOT_DIR}/$${d%/*} && \
		cp -R ${STAGE}/$$d ${SYSROOT_DIR}/$${d%/*}/ || exit 1; \
	done
.if !empty(EXTLIB_LICENSES)
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/${EXTLIB}
	cd ${SRC} && install -m 0444 ${EXTLIB_LICENSES} ${SYSROOT_DIR}/usr/share/licenses/${EXTLIB}/
.endif
.endif

clean: .PHONY
	rm -rf ${SRC} ${BUILD} ${STAGE} ${CROSS} ${LDWRAP} ${CMAKE_TC} ${.OBJDIR}/setup.log ${.OBJDIR}/stage.log

.include <bsd.prog.mk>
