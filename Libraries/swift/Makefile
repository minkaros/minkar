# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Swift 6.0's runtime and standard library for Minkar (Phase 3):
# /usr/lib/swift/libswiftCore.dylib, and the Swift module the compiler
# imports (in the toolchain's lib/swift). The same Swift tree as the
# compiler (Developer/Default.xctoolchain/swift: MANIFEST, patches/),
# cross-compiled from the Linux build host with the toolchain's clang
# and ld64 against Minkar.sdk (darwin-toolchain.cmake.in), and the
# compiler built there. Swift's CMake asks xcrun, defaults and xcodebuild
# about the SDK: shims/ answers for them.
#
# Built after libSystem and objc4, which it links against, by the
# Frameworks stage (Frameworks/Makefile), after Foundation: its Objective-C
# parts include Foundation's headers.

SUBDIR =
SWIFT =		${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/swift
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
LLVM_SRC =	${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/llvm
LLVM_HOST =	${ROOT_BINARY_DIR}/work/minkar/Developer/Default.xctoolchain/llvm_host

PATCHES =	0001-no-astgen-without-swift-syntax.patch

SRC =		${.OBJDIR}/swift-src
BUILD =		${.OBJDIR}/build
SHIMS =		${.OBJDIR}/shims
CROSS =		${.OBJDIR}/darwin-toolchain.cmake
LIBDIR =	${BUILD}/lib/swift/macosx/x86_64
LIBS =		libswiftCore.dylib libswiftDarwin.dylib libswift_Concurrency.dylib
TARGETS =	swiftCore-macosx-x86_64 swiftDarwin-macosx-x86_64 swift_Concurrency-macosx-x86_64

TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET PATH=${SHIMS}:$${PATH}

all: install-swift-runtime

${SRC}/.minkar-patches: ${PATCHES:S,^,${SWIFT}/patches/,} ${SWIFT}/MANIFEST
	pristine=$$(${FETCH} ${SWIFT}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${SWIFT}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

${CROSS}: ${.CURDIR}/darwin-toolchain.cmake.in ${.CURDIR}/shims/xcrun.in
	mkdir -p ${SHIMS}
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${MINKAR_SDKROOT}|g' ${.CURDIR}/shims/xcrun.in \
	    > ${SHIMS}/xcrun
	install -m 0755 ${.CURDIR}/shims/defaults ${.CURDIR}/shims/xcodebuild ${SHIMS}/
	chmod 0755 ${SHIMS}/xcrun
	ln -sf ${TC}/ld64 ${SHIMS}/ld
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${MINKAR_SDKROOT}|g' \
	    ${.CURDIR}/darwin-toolchain.cmake.in > ${.TARGET}

# The core and the Darwin overlay (on the SDK's Darwin module).
# Concurrency (async/await, tasks, actors) on the system's libdispatch:
# it needs the compiler built with swift-syntax (#isolation).
# objc_setHook_lazyClassNamer is API 20: weakly imported, left undefined.
${BUILD}/.minkar-setup: ${SRC}/.minkar-patches ${CROSS} ${.CURDIR}/Makefile
	rm -rf ${BUILD}
	${TOOLENV} cmake -G Ninja -S ${SRC} -B ${BUILD} \
	    -DCMAKE_TOOLCHAIN_FILE=${CROSS} -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-U,_objc_setHook_lazyClassNamer" \
	    -DLLVM_DIR=${LLVM_HOST}/lib/cmake/llvm -DClang_DIR=${LLVM_HOST}/lib/cmake/clang \
	    -DSWIFT_PATH_TO_LLVM_SOURCE=${LLVM_SRC}/llvm \
	    -DSWIFT_PATH_TO_CLANG_SOURCE=${LLVM_SRC}/clang \
	    -DSWIFT_INCLUDE_TOOLS=OFF -DSWIFT_BUILD_STDLIB=ON -DSWIFT_BUILD_SDK_OVERLAY=ON \
	    -DSWIFT_BUILD_DYNAMIC_STDLIB=ON -DSWIFT_BUILD_STATIC_STDLIB=OFF \
	    -DSWIFT_INCLUDE_TESTS=OFF -DSWIFT_INCLUDE_DOCS=OFF -DSWIFT_BUILD_REMOTE_MIRROR=OFF \
	    -DSWIFT_BUILD_SOURCEKIT=OFF \
	    -DSWIFT_HOST_VARIANT_SDK=OSX -DSWIFT_HOST_VARIANT_ARCH=x86_64 -DSWIFT_SDKS=OSX \
	    -DSWIFT_PRIMARY_VARIANT_SDK=OSX -DSWIFT_PRIMARY_VARIANT_ARCH=x86_64 \
	    -DSWIFT_DARWIN_SUPPORTED_ARCHS=x86_64 -DSWIFT_SDK_OSX_PATH=${MINKAR_SDKROOT} \
	    -DSWIFT_DARWIN_DEPLOYMENT_VERSION_OSX=10.15 \
	    -DSWIFT_NATIVE_SWIFT_TOOLS_PATH=${TC} -DSWIFT_NATIVE_CLANG_TOOLS_PATH=${TC} \
	    -DSWIFT_NATIVE_LLVM_TOOLS_PATH=${TC} \
	    -DBOOTSTRAPPING_MODE=OFF -DSWIFT_BUILD_SWIFT_SYNTAX=OFF \
	    -DSWIFT_ENABLE_EXPERIMENTAL_CONCURRENCY=ON -DSWIFT_ENABLE_DISPATCH=ON \
	    -DSWIFT_CONCURRENCY_GLOBAL_EXECUTOR=dispatch \
	    -DSWIFT_ENABLE_EXPERIMENTAL_DISTRIBUTED=OFF \
	    -DSWIFT_ENABLE_EXPERIMENTAL_STRING_PROCESSING=OFF \
	    -DSWIFT_ENABLE_EXPERIMENTAL_OBSERVATION=OFF -DSWIFT_ENABLE_SYNCHRONIZATION=OFF \
	    -DSWIFT_ENABLE_VOLATILE=OFF -DSWIFT_STDLIB_ENABLE_CXX_INTEROP=OFF \
	    -DSWIFT_BUILD_STDLIB_CXX_MODULE=OFF -DSWIFT_STDLIB_BUILD_PRIVATE=OFF \
	    -DSWIFT_BUILD_STDLIB_EXTRA_TOOLCHAIN_CONTENT=OFF \
	    > ${.OBJDIR}/stdlib-config.log 2>&1 || { tail -30 ${.OBJDIR}/stdlib-config.log; exit 1; }
	touch ${.TARGET}

build-swift-runtime: .PHONY ${BUILD}/.minkar-setup
	${TOOLENV} ninja -j${JOBS:U10} -C ${BUILD} ${TARGETS} \
	    > ${.OBJDIR}/stdlib-build.log 2>&1 || \
	    { grep -A8 FAILED ${.OBJDIR}/stdlib-build.log | head -40; exit 1; }

install-swift-runtime: .PHONY build-swift-runtime
	mkdir -p ${SYSROOT_DIR}/usr/lib/swift ${MINKAR_SDKROOT}/usr/lib/swift \
	    ${TOOLCHAIN}/usr/lib/swift ${SYSROOT_DIR}/usr/share/swift
.for l in ${LIBS}
	install -m 0755 ${LIBDIR}/${l} ${SYSROOT_DIR}/usr/lib/swift/${l}
	install -m 0755 ${LIBDIR}/${l} ${MINKAR_SDKROOT}/usr/lib/swift/${l}
.endfor
	# The modules and shims the compiler imports, in its resource directory.
	cp -R ${BUILD}/lib/swift/. ${TOOLCHAIN}/usr/lib/swift/
	# Swift's own API notes (Dispatch, os), in the compiler's resource directory.
	mkdir -p ${TOOLCHAIN}/usr/lib/swift/apinotes
	cp -f ${SRC}/apinotes/*.apinotes ${TOOLCHAIN}/usr/lib/swift/apinotes/
	install -m 0444 ${SRC}/LICENSE.txt ${SYSROOT_DIR}/usr/share/swift/LICENSE.txt

clean: .PHONY
	rm -rf ${SRC} ${BUILD} ${SHIMS} ${CROSS} ${.OBJDIR}/*.log

.include <bsd.prog.mk>
