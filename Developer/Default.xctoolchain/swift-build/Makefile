# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Swift Build, for the build host: swbuild, its build service
# (SWBBuildServiceBundle) and their specs (the SwiftBuild_*.bundle
# folders), in the toolchain's libexec/swift-build, with swbuild linked
# from bin. It plans and runs the build of an Xcode project from its PIF,
# which xcodebuild -dumpPIF (../xcbuild) writes.
#
# Swift Build (MANIFEST) and the packages it is built from (MANIFEST.*)
# are fetched at their pinned commits, the tag swift-6.4.2-RELEASE or a
# release of their own; Swift Build is patched here (patches/). They are
# built by the Swift for the build host (../swift/MANIFEST.host-swift),
# with SwiftPM, each package beside the others as SWIFTCI_USE_LOCAL_DEPS
# expects. They load that Swift's Linux runtime and Foundation, which
# ../swift installs in the toolchain's lib/swift/linux. (Not linked in
# statically: the runtime finds Foundation's bridging, an Array as an
# NSArray, by looking up its symbols, which a static executable hides.)
#
# The host needs libsqlite3-dev (llbuild links the system's SQLite).

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
SWIFT_DIR =	${.CURDIR}/../swift

DEPS =		llbuild swift-driver swift-tools-protocols swift-tools-support-core \
		swift-system swift-argument-parser swift-toolchain-sqlite
PATCHES =	0001-lsregister-only-on-the-reference-host.patch \
		0002-console-prints-session-diagnostics.patch

WORK =		${.OBJDIR}/packages
SRC =		${WORK}/swift-build
DEST =		${TOOLCHAIN}/usr/libexec/swift-build

all: install-swift-build

# The packages side by side: Swift Build copied and patched, the others
# linked from the cache. Made again when a manifest or a patch changes.
${WORK}/.minkar-setup: ${.CURDIR}/MANIFEST ${DEPS:@d@${.CURDIR}/MANIFEST.${d}@} \
    ${PATCHES:@p@${.CURDIR}/patches/${p}@}
	rm -rf ${WORK} && mkdir -p ${WORK}
	for d in ${DEPS}; do \
		dir=$$(${FETCH} ${.CURDIR}/MANIFEST.$$d ${CACHE}) || exit 1; \
		ln -s $$dir ${WORK}/$$d || exit 1; \
	done
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rsync -a --exclude=.git $$pristine/ ${SRC}.part/ && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

build-swift-build: .PHONY ${WORK}/.minkar-setup
	hostswift=$$(${FETCH} ${SWIFT_DIR}/MANIFEST.host-swift ${CACHE}) && \
	cd ${SRC} && env PATH=$$hostswift/usr/bin:$$PATH SWIFTCI_USE_LOCAL_DEPS=1 \
	    swift build -c release --product swbuild --disable-local-rpath \
	    -Xswiftc -no-toolchain-stdlib-rpath \
	    -Xlinker -rpath -Xlinker '$$ORIGIN/../../lib/swift/linux' -j ${JOBS:U${.MAKE.JOBS:U10}} \
	    > ${.OBJDIR}/swift-build.log 2>&1 || \
	    { grep -B2 -A5 'error:' ${.OBJDIR}/swift-build.log | head -40; exit 1; }

# Built again only when what it is built from changes, by content.
STAMP =		${DEST}/.minkar-built
SUM_CMD =	cat ${.CURDIR}/MANIFEST ${DEPS:@d@${.CURDIR}/MANIFEST.${d}@} \
		    ${SWIFT_DIR}/MANIFEST.host-swift ${.CURDIR}/Makefile \
		    ${PATCHES:@p@${.CURDIR}/patches/${p}@} | cksum

install-swift-build: .PHONY
	@if [ -x ${DEST}/swbuild ] && [ -f ${STAMP} ] && \
	    [ "$$(${SUM_CMD})" = "$$(cat ${STAMP})" ]; then \
		echo "swift-build: the toolchain's Swift Build is current"; \
	else \
		${MAKE} -C ${.CURDIR} install-swift-build-now; \
	fi

install-swift-build-now: .PHONY build-swift-build
	mkdir -p ${DEST} ${TOOLCHAIN}/usr/bin ${TOOLCHAIN}/usr/share/swift-build
	# The service is found beside swbuild, and the specs beside the
	# executable that loads them.
	# Stripped here first: install -C compares, and does not strip.
.for t in swbuild SWBBuildServiceBundle
	strip -o ${.OBJDIR}/${t}.stripped ${SRC}/.build/release/${t}
	install -C -m 0755 ${.OBJDIR}/${t}.stripped ${DEST}/${t}
.endfor
	rsync -rlc --delete --include='SwiftBuild_*.bundle/***' --exclude='*' \
	    ${SRC}/.build/release/ ${DEST}/
	ln -sf ../libexec/swift-build/swbuild ${TOOLCHAIN}/usr/bin/swbuild
	install -C -m 0444 ${SRC}/LICENSE.txt ${TOOLCHAIN}/usr/share/swift-build/LICENSE.txt
	for d in ${DEPS}; do \
		for f in LICENSE.txt LICENSE; do \
			[ -f ${WORK}/$$d/$$f ] || continue; \
			install -C -m 0444 ${WORK}/$$d/$$f \
			    ${TOOLCHAIN}/usr/share/swift-build/$$d-$$f || exit 1; \
		done; \
	done
	${SUM_CMD} > ${STAMP}

clean: .PHONY
	rm -rf ${WORK} ${.OBJDIR}/*.log

.include <bsd.prog.mk>
