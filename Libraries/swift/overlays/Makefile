# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Swift's overlays for the system's C and Objective-C modules (Phase 3):
# ObjectiveC, Dispatch and CoreFoundation, from Swift 5.4, the last
# release that has them in the open (MANIFEST; our patches in patches/),
# built with the toolchain's Swift 6.0. Each is a library evolution
# module (its .swiftinterface beside the .swiftmodule) next to Darwin's
# in the compiler's resource directory, and a /usr/lib/swift dylib that
# programs link through the module's link name. Built from
# Frameworks/Makefile, after CoreFoundation, whose module the
# CoreFoundation overlay extends.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
RESOURCE =	${TOOLCHAIN}/usr/lib/swift/macosx
TRIPLE =	x86_64-apple-macosx10.15
MODTRIPLE =	x86_64-apple-macos

PATCHES =	0001-dispatch-without-runtime-headers.patch
SRC =		${.OBJDIR}/swift54
DARWIN =	${SRC}/stdlib/public/Darwin
OUT =		${.OBJDIR}/out
SHIMS =		${.OBJDIR}/overlay-shims

# In dependency order. Dispatch's Combine schedulers wait for Combine.
OVERLAYS =	ObjectiveC Dispatch CoreFoundation
SOURCES_ObjectiveC =	ObjectiveC.swift
SOURCES_Dispatch =	Dispatch.swift Block.swift Data.swift IO.swift Private.swift \
			Queue.swift Source.swift Time.swift
OBJC_Dispatch =		Dispatch.mm
SOURCES_CoreFoundation = CoreFoundation.swift
FLAGS_ObjectiveC =	-disable-objc-attr-requires-foundation-module
LIBS_ObjectiveC =	-lobjc
# Dispatch retains the blocks it is given: CoreFoundation makes them
# Objective-C objects (NSBlock.m).
LIBS_Dispatch =		-lobjc -framework CoreFoundation
LIBS_CoreFoundation =	-framework CoreFoundation

TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET

all: install-overlays

${SRC}/.minkar-patches: ${PATCHES:S,^,${.CURDIR}/patches/,} ${.CURDIR}/MANIFEST
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && mkdir -p ${SRC}.part/stdlib/public && \
	cp -a $$pristine/stdlib/public/Darwin $$pristine/stdlib/public/SwiftShims \
	    ${SRC}.part/stdlib/public/ && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The overlays' own C headers (imported @_implementationOnly: only for
# building them).
${SHIMS}/module.modulemap: ${SRC}/.minkar-patches
	rm -rf ${SHIMS} && mkdir -p ${SHIMS}
	cp ${SRC}/stdlib/public/SwiftShims/*OverlayShims.h ${SRC}/stdlib/public/SwiftShims/CF*Shims.h \
	    ${SRC}/stdlib/public/SwiftShims/NS*Shims.h \
	    ${SRC}/stdlib/public/SwiftShims/FoundationShimSupport.h ${SHIMS}/
	awk '/^module _Swift[A-Za-z]*OverlayShims/,/^}/' \
	    ${SRC}/stdlib/public/SwiftShims/module.modulemap > ${.TARGET}

.for o in ${OVERLAYS}
${OUT}/${o}.o: ${SHIMS}/module.modulemap ${SOURCES_${o}:S,^,${DARWIN}/${o}/,} ${.CURDIR}/Makefile
	mkdir -p ${OUT}/${o}.swiftmodule
	${TOOLENV} ${TC}/swiftc -frontend -c -O -parse-as-library -enable-library-evolution \
	    -module-name ${o} -module-link-name swift${o} -target ${TRIPLE} \
	    -sdk ${MINKAR_SDKROOT} -runtime-compatibility-version none \
	    -I ${SHIMS} -I ${OUT} -module-cache-path ${.OBJDIR}/module-cache ${FLAGS_${o}} \
	    -emit-module-path ${OUT}/${o}.swiftmodule/${MODTRIPLE}.swiftmodule \
	    -emit-module-interface-path ${OUT}/${o}.swiftmodule/${MODTRIPLE}.swiftinterface \
	    -emit-module-doc-path ${OUT}/${o}.swiftmodule/${MODTRIPLE}.swiftdoc \
	    ${SOURCES_${o}:S,^,${DARWIN}/${o}/,} -o ${.TARGET}

${OUT}/libswift${o}.dylib: ${OUT}/${o}.o ${.CURDIR}/Makefile
	cd ${OUT} && for m in ${OBJC_${o}}; do \
		${TOOLENV} ${TC}/clang -c -O2 -fobjc-arc -target ${TRIPLE} \
		    -isysroot ${MINKAR_SDKROOT} -Wno-incompatible-sysroot \
		    ${DARWIN}/${o}/$$m -o ${o}-$${m%.*}.o || exit 1; \
	done
	${TOOLENV} ${TC}/clang -dynamiclib -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
	    -fuse-ld=${TC}/ld64 -Wno-incompatible-sysroot \
	    -install_name /usr/lib/swift/libswift${o}.dylib \
	    ${OUT}/${o}.o ${OBJC_${o}:R:S,^,${OUT}/${o}-,:S,$,.o,} \
	    -L${MINKAR_SDKROOT}/usr/lib/swift -L${OUT} ${LIBS_${o}} -o ${.TARGET}
.endfor

install-overlays: .PHONY ${OVERLAYS:S,^,${OUT}/libswift,:S,$,.dylib,}
	mkdir -p ${SYSROOT_DIR}/usr/lib/swift ${MINKAR_SDKROOT}/usr/lib/swift ${RESOURCE} \
	    ${SYSROOT_DIR}/usr/share/swift
.for o in ${OVERLAYS}
	install -m 0755 ${OUT}/libswift${o}.dylib ${SYSROOT_DIR}/usr/lib/swift/
	install -m 0755 ${OUT}/libswift${o}.dylib ${MINKAR_SDKROOT}/usr/lib/swift/
	rm -rf ${RESOURCE}/${o}.swiftmodule
	cp -R ${OUT}/${o}.swiftmodule ${RESOURCE}/
.endfor

clean: .PHONY
	rm -rf ${SRC} ${SHIMS} ${OUT} ${.OBJDIR}/module-cache

.include <bsd.prog.mk>
