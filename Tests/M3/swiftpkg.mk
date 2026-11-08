# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# A Swift package built without the package manager, for gate M3: its
# targets compiled with the toolchain's swiftc against Minkar.sdk, each
# module whole (one object), libraries linked statically into the
# programs that use them. Test targets become test programs: their main
# is written by xctest-discover, and they link XCTest.framework.
#
# A tool's Makefile sets, then includes this file:
#   PKG             its name (the directory in /usr/local/tests/m3)
#   MANIFEST        the pinned tarball (fetch.sh)
#   DEPPKGS         packages it depends on, each with MANIFEST_<p>: put in
#                   .deps/<p>, their targets named with DIR_<t>
#   CLIBS           C library targets (their headers and module map in
#                   include/, as the package manager finds them)
#   LIBS            library targets, in dependency order
#   PROGS           executable targets
#   TESTS           test targets
#   DIR_<t>         a target's sources, in the package (default Sources/<t>,
#                   Tests/<t> for tests)
#   DEPS_<t>        the libraries it imports, in dependency order
#   FLAGS_<t>       more swiftc flags for it
#   SKIP_<t>        tests a test target leaves out (Class.testMethod), each
#                   with its reason in the package's Makefile
#   CFLAGS_<t>      more C compiler flags for a C target
#   RES_<t>         resources of a target, in the package: copied into
#                   <PKG>_<t>.bundle next to the programs, and Bundle.module
#                   (as the package manager writes it) finds them there
#   DATA            files or folders of the package the tests read by
#                   their path in it, installed at the same path
#   RUN_ENV         VAR=value words the tests run with (run.env, read by
#                   run.sh)
#   RELATIVE_<t>    compile a test target's sources by their paths in the
#                   package, so that #file is relative: tests that find the
#                   package's files from it find them in the installed copy
#                   (run.sh runs tests from their package's directory)
#   PATCHES         patches/ files applied to the package
#   PROGNAME_<t>    the name a program is installed under (default <t>)
#   GIT_REPO        yes: the package is made a git repository (one commit,
#                   with its own user name and e-mail), for tests that ask
#                   git about its files
# Programs and test programs are installed together, so tests that run
# the package's programs find them next to themselves.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
TRIPLE =	x86_64-apple-macosx10.15
XCTEST_FMWKS =	${MINKAR_SDKROOT}/../../Library/Frameworks
SRC =		${.OBJDIR}/pkg
MODS =		${.OBJDIR}/modules
OUT =		${.OBJDIR}/out
DEST =		${SYSROOT_DIR}/usr/local/tests/m3/${PKG}
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET
# Built as the package manager builds a package for its tests: debug
# configuration (DEBUG, optimized all the same), SWIFT_PACKAGE, and the
# package name for `package` declarations.
SWIFT_THREADS ?= 4
SWIFTC =	${TOOLENV} ${TC}/swiftc -frontend -c -O -target ${TRIPLE} -D SWIFT_PACKAGE -D DEBUG \
		-package-name ${PKG:S/-/_/g} \
		-sdk ${MINKAR_SDKROOT} -runtime-compatibility-version none \
		-module-cache-path ${.OBJDIR}/module-cache -I ${MODS} -F ${XCTEST_FMWKS}
LINK =		${TOOLENV} ${TC}/clang -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
		-fuse-ld=${TC}/ld64 -Wno-incompatible-sysroot \
		-L${MINKAR_SDKROOT}/usr/lib/swift -Wl,-rpath,/usr/lib/swift

all: install-pkg

${SRC}/.minkar-patches: ${PATCHES:S,^,${.CURDIR}/patches/,} ${MANIFEST} ${.CURDIR}/Makefile \
	    ${.CURDIR}/../swiftpkg.mk \
	    ${DEPPKGS:@d@${.CURDIR}/${MANIFEST_${d}}@}
	pristine=$$(${FETCH} ${MANIFEST} ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part
.for d in ${DEPPKGS}
	dep=$$(${FETCH} ${.CURDIR}/${MANIFEST_${d}} ${CACHE}) && \
	mkdir -p ${SRC}.part/.deps && cp -a $$dep ${SRC}.part/.deps/${d}
.endfor
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done
.if defined(GIT_REPO)
	cd ${SRC}.part && git init -q && git config user.name Minkar && \
	    git config user.email minkar@localhost && git add -A && \
	    GIT_AUTHOR_DATE=2026-01-01T00:00:00Z GIT_COMMITTER_DATE=2026-01-01T00:00:00Z \
	    git -c user.name=Minkar -c user.email=minkar@localhost commit -q -m "${PKG} as fetched"
.endif
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

.for t in ${CLIBS} ${LIBS} ${PROGS}
DIR_${t} ?=	Sources/${t}
.endfor
# What a dependency gives the targets that use it: the object (or the C
# library) linked in, and a C target's headers.
.for t in ${LIBS}
OBJ_${t} =	${OUT}/${t}.o
.endfor
.for t in ${CLIBS}
OBJ_${t} =	${OUT}/lib${t}.a
INC_${t} =	-I ${SRC}/${DIR_${t}}/include
.endfor

# C libraries: each source compiled, the objects in a static library.
.for t in ${CLIBS}
${OUT}/lib${t}.a: ${SRC}/.minkar-patches
	rm -rf ${OUT}/${t}.c && mkdir -p ${OUT}/${t}.c
	for f in $$(find ${SRC}/${DIR_${t}} -name '*.c' | sort); do \
		${TOOLENV} ${TC}/clang -c -O2 -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
		    -Wno-incompatible-sysroot ${INC_${t}} -I $$(dirname $$f) ${CFLAGS_${t}} \
		    $$f -o ${OUT}/${t}.c/$$(basename $$f .c).o || exit 1; \
	done
	rm -f ${.TARGET} && ${TC}/llvm-ar rcs ${.TARGET} ${OUT}/${t}.c/*.o
.endfor
.for t in ${TESTS}
DIR_${t} ?=	Tests/${t}
.endfor

# Libraries: a module and an object each, built for @testable import.
.for t in ${LIBS}
# Its code is generated on SWIFT_THREADS threads, an object for each
# source (the whole module is one job: SwiftFormat's took seven minutes on
# one), then joined into one.
${OUT}/${t}.o: ${SRC}/.minkar-patches ${DEPS_${t}:@d@${OBJ_${d}}@}
	mkdir -p ${MODS} ${OUT}
	rm -rf ${OUT}/${t}.parts && mkdir -p ${OUT}/${t}.parts
	find ${SRC}/${DIR_${t}} -name '*.swift' -print0 | sort -z | xargs -0 sh -c \
	    'i=0; for f in "$$@"; do case $$f in *.swift) set -- "$$@" -o ${OUT}/${t}.parts/$$i.o; i=$$((i + 1));; esac; done; \
	     exec "$$@"' sh \
	    ${SWIFTC} -num-threads ${SWIFT_THREADS} -parse-as-library -enable-testing -module-name ${t} ${FLAGS_${t}} \
	    ${DEPS_${t}:@d@${INC_${d}}@} \
	    -emit-module-path ${MODS}/${t}.swiftmodule
	${TOOLENV} ${TC}/ld64 -r -arch x86_64 ${OUT}/${t}.parts/*.o -o ${.TARGET}
.endfor

# Programs: a main.swift is top-level code; without one, @main.
.for t in ${PROGS}
${OUT}/${t}: ${SRC}/.minkar-patches ${DEPS_${t}:@d@${OBJ_${d}}@}
	mkdir -p ${OUT}
	lib=-parse-as-library; \
	[ -n "$$(find ${SRC}/${DIR_${t}} -name main.swift)" ] && lib=; \
	find ${SRC}/${DIR_${t}} -name '*.swift' -print0 | sort -z | xargs -0 \
	    ${SWIFTC} $$lib -module-name ${t:S/-/_/g} ${FLAGS_${t}} ${DEPS_${t}:@d@${INC_${d}}@} -o ${.TARGET}.o
	${LINK} ${.TARGET}.o ${DEPS_${t}:@d@${OBJ_${d}}@} -o ${.TARGET}
.endfor

# Test programs: XCTMain over the tests xctest-discover finds.
.for t in ${TESTS}
${OUT}/${t}: ${SRC}/.minkar-patches ${DEPS_${t}:@d@${OBJ_${d}}@}
	mkdir -p ${OUT}/${t}.main
.if !empty(RES_${t})
	rm -rf ${OUT}/${PKG}_${t}.bundle && mkdir -p ${OUT}/${PKG}_${t}.bundle
	cp -R ${RES_${t}:S,^,${SRC}/,} ${OUT}/${PKG}_${t}.bundle/
	printf '%s\n' 'import Foundation' 'extension Foundation.Bundle {' \
	    '    static let module: Bundle = Bundle(path: Bundle.main.bundleURL.appendingPathComponent("${PKG}_${t}.bundle").path)!' \
	    '}' > ${OUT}/${t}.main/resource_bundle_accessor.swift
.endif
	find ${SRC}/${DIR_${t}} -name '*.swift' -print0 | sort -z | xargs -0 \
	    ${TC}/xctest-discover ${SKIP_${t}:@s@--skip ${s}@} -o ${OUT}/${t}.main/main.swift
.if defined(RELATIVE_${t})
	cd ${SRC} && { find ${OUT}/${t}.main -name '*.swift' -print0; \
	  find ${DIR_${t}} -name '*.swift' -print0 | sort -z; } | xargs -0 \
	    ${SWIFTC} -module-name ${t} ${FLAGS_${t}} ${DEPS_${t}:@d@${INC_${d}}@} -o ${.TARGET}.o
.else
	{ find ${OUT}/${t}.main -name '*.swift' -print0; \
	  find ${SRC}/${DIR_${t}} -name '*.swift' -print0 | sort -z; } | xargs -0 \
	    ${SWIFTC} -module-name ${t} ${FLAGS_${t}} ${DEPS_${t}:@d@${INC_${d}}@} -o ${.TARGET}.o
.endif
	${LINK} ${.TARGET}.o ${DEPS_${t}:@d@${OBJ_${d}}@} -F ${XCTEST_FMWKS} \
	    -Wl,-rpath,/Library/Developer/Frameworks -o ${.TARGET}
.endfor

install-pkg: .PHONY ${PROGS:S,^,${OUT}/,} ${TESTS:S,^,${OUT}/,}
	rm -rf ${DEST} && mkdir -p ${DEST}
.for t in ${PROGS} ${TESTS}
	install -m 0755 ${OUT}/${t} ${DEST}/${PROGNAME_${t}:U${t}}
.if !empty(RES_${t})
	cp -R ${OUT}/${PKG}_${t}.bundle ${DEST}/
.endif
.endfor
	cp ${SRC}/LICENSE* ${DEST}/ 2>/dev/null || true
.for d in ${DATA}
	mkdir -p ${DEST}/${d:H}
	cp -R ${SRC}/${d} ${DEST}/${d:H}/
.endfor
.if !empty(RUN_ENV)
	printf '%s\n' ${RUN_ENV} > ${DEST}/run.env
.endif

clean: .PHONY
	rm -rf ${SRC} ${MODS} ${OUT} ${.OBJDIR}/module-cache

.include <bsd.prog.mk>
