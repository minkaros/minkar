# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# XCTest.framework from swift-corelibs-xctest (MANIFEST), built with the
# toolchain's Swift 6.0 on Minkar's Foundation: a library evolution
# module (its .swiftinterface in the framework's Modules) and the
# framework's dylib, @rpath/XCTest.framework as test programs find it.
# It is a developer framework, as on the reference platform: in the
# platform's Developer/Library/Frameworks, next to the SDK, for building
# tests, and in the image's /Library/Developer/Frameworks for running
# them. The license goes with it, in Resources/LICENSE.XCTest.
#
# Test programs have no bundle loader: each is an executable whose main
# is XCTMain, written by xctest-discover.py from the test sources
# (installed in the toolchain as xctest-discover).
#
# On the sources: the concurrency they mark as API 21 (12.0) is in
# the system's Swift runtime, so the marks go.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
TRIPLE =	x86_64-apple-macosx10.15
MODTRIPLE =	x86_64-apple-macos

SRC =		${.OBJDIR}/xctest
FMWK =		${.OBJDIR}/XCTest.framework
VERS =		${FMWK}/Versions/A
MODULE =	${VERS}/Modules/XCTest.swiftmodule
PLATFORM_FMWKS = ${MINKAR_SDKROOT}/../../Library/Frameworks
IMAGE_FMWKS =	${SYSROOT_DIR}/Library/Developer/Frameworks

TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET

all: install-xctest

${SRC}/.minkar-patches: ${.CURDIR}/MANIFEST ${.CURDIR}/Makefile
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && mkdir -p ${SRC}.part && \
	cp -a $$pristine/Sources $$pristine/LICENSE ${SRC}.part/ && \
	find ${SRC}.part/Sources -name '*.swift' -exec sed -i -E \
	    -e '/^ *@available\(macOS 12\.0, \*\) *$$/d' {} + && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The class XCTest is named after its module: the interface spells the
# module's names apart (-alias-module-names-in-module-interface). Built
# again when Foundation's overlay is (it is rebuilt with Foundation's
# headers).
${.OBJDIR}/XCTest.o: ${SRC}/.minkar-patches ${MINKAR_SDKROOT}/usr/lib/swift/libswiftFoundation.dylib
	rm -rf ${MODULE} && mkdir -p ${MODULE}
	${TOOLENV} ${TC}/swiftc -frontend -c -O -parse-as-library -enable-library-evolution \
	    -module-name XCTest -target ${TRIPLE} \
	    -sdk ${MINKAR_SDKROOT} -runtime-compatibility-version none \
	    -D USE_FOUNDATION_FRAMEWORK -alias-module-names-in-module-interface \
	    -module-cache-path ${.OBJDIR}/module-cache \
	    -emit-module-path ${MODULE}/${MODTRIPLE}.swiftmodule \
	    -emit-module-interface-path ${MODULE}/${MODTRIPLE}.swiftinterface \
	    -emit-module-doc-path ${MODULE}/${MODTRIPLE}.swiftdoc \
	    $$(find ${SRC}/Sources/XCTest -name '*.swift' | sort) -o ${.TARGET}

${VERS}/XCTest: ${.OBJDIR}/XCTest.o ${.CURDIR}/Info.plist
	mkdir -p ${VERS}/Resources
	${TOOLENV} ${TC}/clang -dynamiclib -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
	    -fuse-ld=${TC}/ld64 -Wno-incompatible-sysroot \
	    -install_name @rpath/XCTest.framework/Versions/A/XCTest \
	    -compatibility_version 1.0.0 -current_version 1.0.0 \
	    ${.OBJDIR}/XCTest.o -L${MINKAR_SDKROOT}/usr/lib/swift -o ${.TARGET}
	cp ${.CURDIR}/Info.plist ${VERS}/Resources/Info.plist
	cp ${SRC}/LICENSE ${VERS}/Resources/LICENSE.XCTest
	ln -sfn A ${FMWK}/Versions/Current
	ln -sfn Versions/Current/XCTest ${FMWK}/XCTest
	ln -sfn Versions/Current/Modules ${FMWK}/Modules
	ln -sfn Versions/Current/Resources ${FMWK}/Resources

install-xctest: .PHONY ${VERS}/XCTest
.for d in ${PLATFORM_FMWKS} ${IMAGE_FMWKS}
	mkdir -p ${d}
	rm -rf ${d}/XCTest.framework
	cp -R ${FMWK} ${d}/
.endfor
	mkdir -p ${TC}
	install -m 0755 ${.CURDIR}/xctest-discover.py ${TC}/xctest-discover

clean: .PHONY
	rm -rf ${SRC} ${FMWK} ${.OBJDIR}/*.o ${.OBJDIR}/module-cache

.include <bsd.prog.mk>
