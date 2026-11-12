# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Combine.framework (API 19) from OpenCombine (MANIFEST), built with the
# toolchain's Swift 6.0 as the Combine module: a library evolution module
# (its .swiftinterface in the framework's Modules) and the framework's
# dylib, with OpenCombine's C++ helpers (locks, identifiers) inside it,
# imported @_implementationOnly so that programs do not need their
# headers. Programs link it through the module (-framework Combine).
# OpenCombine's license goes with it, in Resources/LICENSE.OpenCombine.
#
# On OpenCombine's sources, besides patches/: OpenCombine.X becomes
# Combine.X, and @inlinable goes (OpenCombine is not written for library
# evolution, which does not let an inlinable initializer set a resilient
# type's stored properties); its GENERATED-*.swift files stand in for
# the gyb templates.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
TRIPLE =	x86_64-apple-macosx10.15
MODTRIPLE =	x86_64-apple-macos

PATCHES =	0001-nested-publishers.patch
SRC =		${.OBJDIR}/opencombine
HELPERS =	${.OBJDIR}/helpers
FMWK =		${.OBJDIR}/Combine.framework
VERS =		${FMWK}/Versions/A
MODULE =	${VERS}/Modules/Combine.swiftmodule

TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET

all: install-combine

${SRC}/.minkar-patches: ${PATCHES:S,^,${.CURDIR}/patches/,} ${.CURDIR}/MANIFEST ${.CURDIR}/Makefile
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && mkdir -p ${SRC}.part/Sources && \
	cp -a $$pristine/Sources/OpenCombine $$pristine/Sources/COpenCombineHelpers \
	    ${SRC}.part/Sources/ && \
	rsync -cL $$pristine/LICENSE ${SRC}.part/ && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	find ${SRC}.part/Sources/OpenCombine -name '*.gyb' -delete && \
	find ${SRC}.part/Sources/OpenCombine -name '*.swift' -exec sed -i -E \
	    -e 's/\bOpenCombine\./Combine./g' \
	    -e 's/^import COpenCombineHelpers$$/@_implementationOnly import COpenCombineHelpers/' \
	    -e 's/@inlinable ?//g' \
	    -e 's/\.OCombine\.Publisher\b/.Publisher/g' {} + && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The helpers' header as a module, for building Combine only.
${HELPERS}/module.modulemap: ${SRC}/.minkar-patches
	rm -rf ${HELPERS} && mkdir -p ${HELPERS}
	rsync -cL ${SRC}/Sources/COpenCombineHelpers/include/COpenCombineHelpers.h ${HELPERS}/
	printf 'module COpenCombineHelpers [system] {\n  header "COpenCombineHelpers.h"\n  export *\n}\n' > ${.TARGET}

${.OBJDIR}/Combine.o: ${HELPERS}/module.modulemap
	rm -rf ${MODULE} && mkdir -p ${MODULE}
	${TOOLENV} ${TC}/swiftc -frontend -c -O -parse-as-library -enable-library-evolution -language-mode 5 \
	    -module-name Combine -target ${TRIPLE} \
	    -sdk ${MINKAR_SDKROOT} -runtime-compatibility-version none \
	    -I ${HELPERS} -module-cache-path ${.OBJDIR}/module-cache \
	    -emit-module-path ${MODULE}/${MODTRIPLE}.swiftmodule \
	    -emit-module-interface-path ${MODULE}/${MODTRIPLE}.swiftinterface \
	    -emit-module-doc-path ${MODULE}/${MODTRIPLE}.swiftdoc \
	    $$(find ${SRC}/Sources/OpenCombine -name '*.swift' | sort) -o ${.TARGET}

${.OBJDIR}/helpers.o: ${HELPERS}/module.modulemap
	${TOOLENV} ${TC}/clang++ -c -O2 -std=c++14 -target ${TRIPLE} \
	    -isysroot ${MINKAR_SDKROOT} -Wno-incompatible-sysroot -I ${HELPERS} \
	    ${SRC}/Sources/COpenCombineHelpers/COpenCombineHelpers.cpp -o ${.TARGET}

${VERS}/Combine: ${.OBJDIR}/Combine.o ${.OBJDIR}/helpers.o ${.CURDIR}/Info.plist
	mkdir -p ${VERS}/Resources
	${TOOLENV} ${TC}/clang -dynamiclib -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
	    -fuse-ld=${TC}/ld64 -Wno-incompatible-sysroot \
	    -install_name /System/Library/Frameworks/Combine.framework/Versions/A/Combine \
	    -compatibility_version 1.0.0 -current_version 1.0.0 \
	    ${.OBJDIR}/Combine.o ${.OBJDIR}/helpers.o \
	    -L${MINKAR_SDKROOT}/usr/lib/swift -lc++ -o ${.TARGET}
	rsync -cL ${.CURDIR}/Info.plist ${VERS}/Resources/Info.plist
	rsync -cL ${SRC}/LICENSE ${VERS}/Resources/LICENSE.OpenCombine
	ln -sfn A ${FMWK}/Versions/Current
	ln -sfn Versions/Current/Combine ${FMWK}/Combine
	ln -sfn Versions/Current/Modules ${FMWK}/Modules
	ln -sfn Versions/Current/Resources ${FMWK}/Resources

install-combine: .PHONY ${VERS}/Combine
.for d in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/System/Library/Frameworks
	rm -rf ${d}/System/Library/Frameworks/Combine.framework
	rsync -rlc ${FMWK} ${d}/System/Library/Frameworks/
.endfor

clean: .PHONY
	rm -rf ${SRC} ${HELPERS} ${FMWK} ${.OBJDIR}/*.o ${.OBJDIR}/module-cache

.include <bsd.prog.mk>
