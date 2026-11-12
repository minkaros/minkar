# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: mogenerator (MANIFEST), an Objective-C tool, and the tests its
# own test directory runs: generate classes for test.xcdatamodel, then
# build a program on them that saves objects in an in-memory store, in
# Objective-C and in Swift. The programs are built here from expected/,
# the classes mogenerator generates on Minkar (they have no dates or
# paths); in the image, mogeneratorTests generates them again, compares
# them with expected/ and runs both programs.

PKG =		mogenerator
MANIFEST =	${.CURDIR}/MANIFEST
SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
TRIPLE =	x86_64-apple-macosx10.15
SRC =		${.OBJDIR}/pkg
OUT =		${.OBJDIR}/out
DEST =		${SYSROOT_DIR}/usr/local/tests/m3/${PKG}
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET
CC_M3 =		${TOOLENV} ${TC}/clang -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
		-Wno-incompatible-sysroot -fmodules -fmodules-cache-path=${.OBJDIR}/module-cache
LINK =		${CC_M3} -fuse-ld=${TC}/ld64
SWIFTC =	${TOOLENV} ${TC}/swiftc -frontend -c -O -target ${TRIPLE} \
		-sdk ${MINKAR_SDKROOT} -runtime-compatibility-version none \
		-module-cache-path ${.OBJDIR}/module-cache
# The tool's sources, as its Xcode project builds them: without ARC.
MOG_DIRS =	. MiscMerge ddcli momcom categories
TEMPLATES =	human.h human.m human.swift machine.h machine.m machine.swift

all: install-pkg

${SRC}/.minkar-patches: ${MANIFEST} ${.CURDIR}/Makefile
	pristine=$$(${FETCH} ${MANIFEST} ${CACHE}) && \
	rm -rf ${SRC} && cp -a $$pristine ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The templates go in the executable's __TEXT,__info_plist section, as
# data under their file names: mogenerator reads them from its bundle.
${OUT}/Info.plist: ${SRC}/.minkar-patches
	mkdir -p ${OUT}
	python3 -c 'import plistlib, sys; \
	    plistlib.dump({n + ".motemplate": open(sys.argv[1] + "/templates/" + n + ".motemplate", "rb").read() \
	        for n in sys.argv[3:]}, open(sys.argv[2], "wb"))' ${SRC} ${.TARGET} ${TEMPLATES}

${OUT}/mogenerator: ${SRC}/.minkar-patches ${OUT}/Info.plist
	rm -rf ${OUT}/mogenerator.o && mkdir -p ${OUT}/mogenerator.o
	for f in $$(cd ${SRC} && find ${MOG_DIRS} -maxdepth 1 -name '*.m' | sort -u); do \
		${CC_M3} -c -O2 -include ${SRC}/mogenerator_Prefix.pch \
		    ${MOG_DIRS:S,^,-I${SRC}/,} ${SRC}/$$f \
		    -o ${OUT}/mogenerator.o/$$(echo $$f | sed "s,^\./,,; s,/,_,g").o || exit 1; \
	done
	${LINK} ${OUT}/mogenerator.o/*.o -framework Foundation -framework CoreData \
	    -sectcreate __TEXT __info_plist ${OUT}/Info.plist -o ${.TARGET}

# test/Rakefile's Objective-C program, on the expected classes.
${OUT}/testbin: ${SRC}/.minkar-patches ${.CURDIR}/expected/objc/_HumanMO.m
	rm -rf ${OUT}/objc && mkdir -p ${OUT}/objc/MOs ${OUT}/objc/objc
	rsync -cL ${.CURDIR}/expected/objc/* ${SRC}/test/objc/Gender.* ${SRC}/test/objc/MyBaseClass.* ${OUT}/objc/MOs/
	cp -R ${SRC}/test/JREnum ${OUT}/objc/
	rsync -cL ${SRC}/test/objc/test.m ${OUT}/objc/objc/
	${LINK} -fobjc-arc -I${OUT}/objc ${OUT}/objc/objc/test.m ${OUT}/objc/MOs/*.m \
	    -framework Foundation -framework AppKit -framework CoreData -o ${.TARGET}

# And its Swift program.
${OUT}/swifttest: ${SRC}/.minkar-patches ${.CURDIR}/expected/swift/_HumanMO.swift
	rm -rf ${OUT}/swift && mkdir -p ${OUT}/swift
	rsync -cL ${.CURDIR}/expected/swift/* ${SRC}/test/swift/*.swift ${OUT}/swift/
	${SWIFTC} -module-name MogenSwiftTest ${OUT}/swift/*.swift -o ${.TARGET}.o
	${LINK} ${.TARGET}.o -L${MINKAR_SDKROOT}/usr/lib/swift -Wl,-rpath,/usr/lib/swift \
	    -framework Foundation -framework CoreData -o ${.TARGET}

install-pkg: .PHONY ${OUT}/mogenerator ${OUT}/testbin ${OUT}/swifttest
	rm -rf ${DEST} && mkdir -p ${DEST}
	install -C -m 0755 ${OUT}/mogenerator ${OUT}/testbin ${OUT}/swifttest ${DEST}/
	install -C -m 0755 ${.CURDIR}/mogeneratorTests.sh ${DEST}/mogeneratorTests
	cp -R ${SRC}/test/test.xcdatamodel ${.CURDIR}/expected ${DEST}/
	rsync -cL ${SRC}/LICENSE ${DEST}/

clean: .PHONY
	rm -rf ${SRC} ${OUT} ${.OBJDIR}/module-cache

.include <bsd.prog.mk>
