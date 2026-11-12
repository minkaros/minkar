# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: tag (MANIFEST), an Objective-C tool that adds, removes, lists
# and finds file tags. It has no tests of its own: tagTests.sh runs each
# of its commands on files it makes, and checks what tag prints and what
# it leaves on the files.

PKG =		tag
MANIFEST =	${.CURDIR}/MANIFEST
PATCHES =	0001-metadata-names.patch
SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
TRIPLE =	x86_64-apple-macosx10.15
SRC =		${.OBJDIR}/pkg
OUT =		${.OBJDIR}/out
DEST =		${SYSROOT_DIR}/usr/local/tests/m3/${PKG}
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET
# As its Xcode project builds it: ARC, Foundation through its prefix.
CC_M3 =		${TOOLENV} ${TC}/clang -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
		-Wno-incompatible-sysroot -fuse-ld=${TC}/ld64 -fobjc-arc -O2

all: install-pkg

${SRC}/.minkar-patches: ${MANIFEST} ${.CURDIR}/Makefile ${PATCHES:S,^,${.CURDIR}/patches/,}
	pristine=$$(${FETCH} ${MANIFEST} ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

${OUT}/tag: ${SRC}/.minkar-patches
	mkdir -p ${OUT}
	${CC_M3} -include ${SRC}/Tag/Tag-Prefix.pch ${SRC}/Tag/main.m ${SRC}/Tag/Tag.m \
	    ${SRC}/Tag/TagName.m -framework Foundation -framework CoreServices -o ${.TARGET}

install-pkg: .PHONY ${OUT}/tag
	rm -rf ${DEST} && mkdir -p ${DEST}
	install -C -m 0755 ${OUT}/tag ${DEST}/
	install -C -m 0755 ${.CURDIR}/tagTests.sh ${DEST}/tagTests
	rsync -cL ${SRC}/LICENSE ${DEST}/

clean: .PHONY
	rm -rf ${SRC} ${OUT}

.include <bsd.prog.mk>
