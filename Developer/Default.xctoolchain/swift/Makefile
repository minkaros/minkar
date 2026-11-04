# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The Swift 6.0 compiler (Phase 3), for the build host: swift-frontend and
# its swiftc and swift links, in the toolchain. Swift (MANIFEST) and
# swift-cmark (MANIFEST.cmark) are fetched at their pinned versions,
# checked against their SHA-256 and patched here (patches/); they are
# built against the toolchain's LLVM and clang (../llvm, built for the
# host in ../llvm_host), the same swift-6.0-RELEASE.
#
# Without swift-syntax (no macros) and without the compiler's own Swift
# modules (no host Swift to build them with). The runtime and standard
# library are built for Minkar in Libraries/swift.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
LLVM_SRC =	${.CURDIR}/../llvm
LLVM_HOST =	${.OBJDIR}/../llvm_host

PATCHES =	0001-no-astgen-without-swift-syntax.patch

SRC =		${.OBJDIR}/swift-src
CMARK =		${.OBJDIR}/cmark-build
BUILD =		${.OBJDIR}/build

# The toolchain's LLVM is built for the host with the old libstdc++ string
# ABI; Swift must be built the same way to link against it.
HOSTFLAGS =	-D_GLIBCXX_USE_CXX11_ABI=0

all: install-swift

# Swift's tree with our patches, made again when the patches change.
${SRC}/.minkar-patches: ${PATCHES:S,^,${.CURDIR}/patches/,} ${.CURDIR}/MANIFEST
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
	ninja -j${JOBS:U10} -C ${CMARK} > ${.OBJDIR}/cmark-build.log 2>&1
	touch ${.TARGET}

${BUILD}/.minkar-setup: ${SRC}/.minkar-patches ${CMARK}/.minkar-built ${.CURDIR}/Makefile
	rm -rf ${BUILD}
	cmake -G Ninja -S ${SRC} -B ${BUILD} -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_C_COMPILER=${HOST_CC:Uclang} -DCMAKE_CXX_COMPILER=${HOST_CXX:Uclang++} \
	    -DCMAKE_CXX_FLAGS="${HOSTFLAGS}" \
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
	    -DBOOTSTRAPPING_MODE=OFF -DSWIFT_BUILD_SWIFT_SYNTAX=OFF \
	    -DSWIFT_ENABLE_EXPERIMENTAL_CONCURRENCY=OFF -DSWIFT_ENABLE_DISPATCH=OFF \
	    -DLLVM_ENABLE_ASSERTIONS=OFF > ${.OBJDIR}/swift-config.log 2>&1 || \
	    { tail -30 ${.OBJDIR}/swift-config.log; exit 1; }
	touch ${.TARGET}

build-swift: .PHONY ${BUILD}/.minkar-setup
	ninja -j${JOBS:U10} -C ${BUILD} swift-frontend > ${.OBJDIR}/swift-build.log 2>&1 || \
	    { grep -A5 FAILED ${.OBJDIR}/swift-build.log | head -40; exit 1; }

# Built again only when what it is built from changes: its manifests,
# patches and this Makefile, by content (a restored cache keeps the
# compiler, not its build tree, and file times are not to be trusted).
STAMP =		${TOOLCHAIN}/usr/share/swift/.minkar-built
SUM_CMD =	cat ${.CURDIR}/MANIFEST ${.CURDIR}/MANIFEST.cmark ${.CURDIR}/Makefile \
		    ${PATCHES:S,^,${.CURDIR}/patches/,} | cksum

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
	install -m 0444 ${SRC}/LICENSE.txt ${TOOLCHAIN}/usr/share/swift/LICENSE.txt
	${SUM_CMD} > ${STAMP}

clean: .PHONY
	rm -rf ${SRC} ${CMARK} ${BUILD} ${.OBJDIR}/*.log

.include <bsd.prog.mk>
