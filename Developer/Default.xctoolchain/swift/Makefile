# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The Swift 6.4.2 compiler (Phase 3), for the build host: swift-frontend and
# its swiftc and swift links, in the toolchain. Swift (MANIFEST) and
# swift-cmark (MANIFEST.cmark) are fetched at their pinned versions,
# checked against their SHA-256 and patched here (patches/); they are
# built against the toolchain's LLVM and clang (../llvm, built for the
# host in ../llvm_host), the same swift-6.4.2-RELEASE.
#
# With swift-syntax (MANIFEST.swift-syntax: macros, #isolation among them)
# and the regex parser of swift-experimental-string-processing
# (MANIFEST.string-processing)
# and the compiler's own Swift modules, built by a Swift for the build
# host (MANIFEST.host-swift, swift.org's release: a build tool only). Their
# C++ interoperation uses the old std::string ABI, as the rest of the
# compiler and the host LLVM do.
# The compiler then loads swift-syntax's libraries and that Swift's Linux
# runtime: both go in the toolchain's lib/swift (host, linux). The runtime
# and standard library for Minkar are built in Libraries/swift.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
LLVM_SRC =	${.CURDIR}/../llvm
LLVM_HOST =	${.OBJDIR}/../llvm_host

# Our patches to Swift (in Developer/Default.xctoolchain/swift/patches); none
# since 6.4.2.
PATCHES =

SRC =		${.OBJDIR}/swift-src
CMARK =		${.OBJDIR}/cmark-build
BUILD =		${.OBJDIR}/build

# The toolchain's LLVM is built for the host with the old libstdc++ string
# ABI; Swift must be built the same way to link against it.
HOSTFLAGS =	-D_GLIBCXX_USE_CXX11_ABI=0

all: install-swift

# Swift's tree with our patches, made again when the patches change.
${SRC}/.minkar-patches: ${PATCHES:@p@${.CURDIR}/patches/${p}@} ${.CURDIR}/MANIFEST
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

${CMARK}/.minkar-built: ${.CURDIR}/MANIFEST.cmark
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.cmark ${CACHE}) && \
	cmake -G Ninja -S $$src -B ${CMARK} -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_C_COMPILER=${HOST_CC:Uclang} -DCMAKE_CXX_COMPILER=${HOST_CXX:Uclang++} \
	    > ${.OBJDIR}/cmark-config.log 2>&1 && \
	ninja -j${JOBS:U${.MAKE.JOBS:U10}} -C ${CMARK} > ${.OBJDIR}/cmark-build.log 2>&1
	touch ${.TARGET}

${BUILD}/.minkar-setup: ${SRC}/.minkar-patches ${CMARK}/.minkar-built ${.CURDIR}/Makefile \
    ${.CURDIR}/MANIFEST.host-swift ${.CURDIR}/MANIFEST.swift-syntax ${.CURDIR}/MANIFEST.string-processing
	rm -rf ${BUILD}
	hostswift=$$(${FETCH} ${.CURDIR}/MANIFEST.host-swift ${CACHE}) && \
	syntax=$$(${FETCH} ${.CURDIR}/MANIFEST.swift-syntax ${CACHE}) && \
	regex=$$(${FETCH} ${.CURDIR}/MANIFEST.string-processing ${CACHE}) && \
	cmake -G Ninja -S ${SRC} -B ${BUILD} -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_Swift_COMPILER=$$hostswift/usr/bin/swiftc \
	    -DSWIFT_PATH_TO_SWIFT_SYNTAX_SOURCE=$$syntax \
	    -DSWIFT_PATH_TO_STRING_PROCESSING_SOURCE=$$regex \
	    -DCMAKE_C_COMPILER=${HOST_CC:Uclang} -DCMAKE_CXX_COMPILER=${HOST_CXX:Uclang++} \
	    -DCMAKE_CXX_FLAGS="${HOSTFLAGS}" \
	    -DCMAKE_Swift_FLAGS="-Xcc -D_GLIBCXX_USE_CXX11_ABI=0" \
	    -DSWIFT_COMPILER_SOURCES_SDK_FLAGS="-Xcc;-D_GLIBCXX_USE_CXX11_ABI=0" \
	    -DLLVM_DIR=${LLVM_HOST}/lib/cmake/llvm -DClang_DIR=${LLVM_HOST}/lib/cmake/clang \
	    -DSWIFT_PATH_TO_LLVM_SOURCE=${LLVM_SRC}/llvm \
	    -DSWIFT_PATH_TO_CLANG_SOURCE=${LLVM_SRC}/clang \
	    -DSWIFT_PATH_TO_CLANG_BUILD=${LLVM_HOST} \
	    -DSWIFT_PATH_TO_CMARK_SOURCE=$$(${FETCH} ${.CURDIR}/MANIFEST.cmark ${CACHE}) \
	    -DSWIFT_PATH_TO_CMARK_BUILD=${CMARK} \
	    -DSWIFT_INCLUDE_TOOLS=ON -DSWIFT_BUILD_STDLIB=OFF -DSWIFT_BUILD_SDK_OVERLAY=OFF \
	    -DSWIFT_BUILD_DYNAMIC_STDLIB=OFF -DSWIFT_BUILD_STATIC_STDLIB=OFF \
	    -DSWIFT_BUILD_STDLIB_EXTRA_TOOLCHAIN_CONTENT=OFF -DSWIFT_BUILD_STDLIB_CXX_MODULE=OFF \
	    -DSWIFT_BUILD_REMOTE_MIRROR=OFF -DSWIFT_INCLUDE_TESTS=OFF -DSWIFT_INCLUDE_DOCS=OFF \
	    -DSWIFT_BUILD_SOURCEKIT=OFF -DSWIFT_ENABLE_SOURCEKIT_TESTS=OFF \
	    -DBOOTSTRAPPING_MODE=HOSTTOOLS -DSWIFT_BUILD_SWIFT_SYNTAX=ON \
	    -DSWIFT_ENABLE_EXPERIMENTAL_CONCURRENCY=OFF -DSWIFT_ENABLE_DISPATCH=OFF \
	    -DLLVM_ENABLE_ASSERTIONS=OFF > ${.OBJDIR}/swift-config.log 2>&1 || \
	    { tail -30 ${.OBJDIR}/swift-config.log; exit 1; }
	touch ${.TARGET}

build-swift: .PHONY ${BUILD}/.minkar-setup
	ninja -j${JOBS:U${.MAKE.JOBS:U10}} -C ${BUILD} swift-frontend > ${.OBJDIR}/swift-build.log 2>&1 || \
	    { grep -A5 FAILED ${.OBJDIR}/swift-build.log | head -40; exit 1; }

# Built again only when what it is built from changes: its manifests,
# patches and this Makefile, by content (a restored cache keeps the
# compiler, not its build tree, and file times are not to be trusted).
STAMP =		${TOOLCHAIN}/usr/share/swift/.minkar-built
SUM_CMD =	cat ${.CURDIR}/MANIFEST ${.CURDIR}/MANIFEST.cmark ${.CURDIR}/MANIFEST.host-swift \
		    ${.CURDIR}/MANIFEST.swift-syntax ${.CURDIR}/MANIFEST.string-processing ${.CURDIR}/Makefile \
		    ${PATCHES:@p@${.CURDIR}/patches/${p}@} | cksum

install-swift: .PHONY
	@if [ -x ${TOOLCHAIN}/usr/bin/swift-frontend ] && [ -f ${STAMP} ] && \
	    [ "$$(${SUM_CMD})" = "$$(cat ${STAMP})" ]; then \
		echo "swift: the toolchain's compiler is current"; \
	else \
		${MAKE} -C ${.CURDIR} install-swift-now; \
	fi

install-swift-now: .PHONY build-swift
	mkdir -p ${TOOLCHAIN}/usr/bin ${TOOLCHAIN}/usr/share/swift
	install -m 0755 ${BUILD}/bin/swift-frontend ${TOOLCHAIN}/usr/bin/swift-frontend
	ln -sf swift-frontend ${TOOLCHAIN}/usr/bin/swiftc
	ln -sf swift-frontend ${TOOLCHAIN}/usr/bin/swift
	# What the compiler loads: swift-syntax's libraries, and the Linux Swift
	# runtime they and the compiler's Swift parts were built against.
	mkdir -p ${TOOLCHAIN}/usr/lib/swift/host/compiler ${TOOLCHAIN}/usr/lib/swift/linux
	cp -f ${BUILD}/lib/swift/host/*.so ${TOOLCHAIN}/usr/lib/swift/host/
	# Since 6.4 the compiler's own swift-syntax libraries are in host/compiler
	# (its RUNPATH).
	cp -f ${BUILD}/lib/swift/host/compiler/*.so ${TOOLCHAIN}/usr/lib/swift/host/compiler/
	# The macro plugins (SwiftMacros: the standard library's @DebugDescription,
	# ObservationMacros).
	mkdir -p ${TOOLCHAIN}/usr/lib/swift/host/plugins
	cp -f ${BUILD}/lib/swift/host/plugins/*.so ${TOOLCHAIN}/usr/lib/swift/host/plugins/
	hostswift=$$(${FETCH} ${.CURDIR}/MANIFEST.host-swift ${CACHE}) && \
	cp -f $$hostswift/usr/lib/swift/linux/*.so ${TOOLCHAIN}/usr/lib/swift/linux/
	install -m 0444 ${SRC}/LICENSE.txt ${TOOLCHAIN}/usr/share/swift/LICENSE.txt
	${SUM_CMD} > ${STAMP}

clean: .PHONY
	rm -rf ${SRC} ${CMARK} ${BUILD} ${.OBJDIR}/*.log

.include <bsd.prog.mk>
