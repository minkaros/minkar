# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Perl 5.40.5 (Artistic or GPL-1.0-or-later, Artistic and Copying;
# perl.org), fetched at build time by MANIFEST and checked against its
# SHA-256: the reference system ships /usr/bin/perl, and so does Minkar
# (the maintainer's choice, 2026-11-11). Installs perl and its tools
# (perldoc, cpan, prove, pod2man, h2xs, xsubpp...), the library with
# libperl.dylib in /usr/lib/perl5 (darwin-thread-multi-2level), the
# manuals and the licences in /usr/share/licenses/perl.
#
# Perl's Configure cross-compiles by running its tests on the target over
# ssh. It was run once that way against a Minkar VM, with Perl's Darwin
# hints; config.sh.in is its config.sh with this build's paths as
# @PERL_SRC@, @SDK@ and @TOOLS@ and the host's names taken out. Here
# Configure -S only turns it into config.h and the Makefile: the build
# needs no VM. To make it again (a new Perl, a changed option), run
# Configure as the first lines of config.sh.in record, with a VM up.
#
# As when Configure cross-compiles, a miniperl for the build host runs the
# build's scripts (host/). patches/ fixes two of Perl's cross-build paths.
# installperl changes libperl's install name only when it runs on Darwin;
# it runs on the build host, so that is done here, and the installed
# Config.pm describes a native perl (cc, no SDK paths, not cross).

all: ${.OBJDIR}/.installed

FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE =		${ROOT_BINARY_DIR}/src-cache
PERL_VERSION !=	awk '$$1 == "version" { print $$2 }' ${.CURDIR}/MANIFEST
JOBS =		-j${.MAKE.JOBS:U1}
GMAKE =		MAKEFLAGS="" ${TOOLS}/gmake
SDK =		${MINKAR_SDKROOT}
TC_BIN =	${TOOLCHAIN}/usr/bin
SRC =		${.OBJDIR}/src
BIN =		${.OBJDIR}/tools
ARCH =		darwin-thread-multi-2level
CORE =		/usr/lib/perl5/${PERL_VERSION}/${ARCH}/CORE

# The target's compiler as config.sh names it, and the answers of two
# commands of the reference system Perl's Darwin hints ask (sw_vers for
# the product version, uname -p for the processor).
${BIN}/minkar-cc: ${.CURDIR}/Makefile
	mkdir -p ${BIN}
	printf '#!/bin/sh\nexec %s --sysroot=%s -mmacos-version-min=%s --ld-path=%s -Qunused-arguments -Wno-nullability-completeness "$$@"\n' \
	    ${CC} ${SDK} ${MACOS_VERSION_MIN} ${LD} > ${BIN}/minkar-cc
	printf '#!/bin/sh\nprintf "ProductName:\\tMinkar\\nProductVersion:\\t%s\\n"\n' \
	    ${MACOS_VERSION_MIN} > ${BIN}/sw_vers
	printf '#!/bin/sh\n[ "$$1" = "-p" ] && { echo i386; exit 0; }\nexec /usr/bin/uname "$$@"\n' > ${BIN}/uname
	chmod 0755 ${BIN}/minkar-cc ${BIN}/sw_vers ${BIN}/uname
	# The extensions' builds run make: the same GNU make (one jobserver).
	ln -sf ${TOOLS}/gmake ${BIN}/make

${SRC}/config.h: ${BIN}/minkar-cc ${.CURDIR}/MANIFEST ${.CURDIR}/config.sh.in \
    ${.CURDIR}/patches/0001-cross-build-for-the-target.patch \
    ${.CURDIR}/patches/0002-makefile-cross-darwin.patch
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${.OBJDIR}/stage && cp -a $$src ${SRC} && chmod -R u+w ${SRC} && \
	for p in ${.CURDIR}/patches/*.patch; do \
	    patch -s -d ${SRC} -p1 < $$p || exit 1; done && \
	mkdir ${SRC}/host && cd ${SRC}/host && \
	env -i PATH=/usr/bin:/bin HOME=${SRC}/host \
	    sh ../Configure -des -Dusedevel -Dmksymlinks > configure.log 2>&1 && \
	env -i PATH=/usr/bin:/bin HOME=${SRC}/host ${TOOLS}/gmake ${JOBS} \
	    miniperl generate_uudmap > build.log 2>&1 && \
	sed -e 's|@PERL_SRC@|${SRC}|g' -e 's|@SDK@|${SDK}|g' -e 's|@TOOLS@|${TC_BIN}|g' \
	    ${.CURDIR}/config.sh.in > ${SRC}/config.sh && \
	cd ${SRC} && PATH=${BIN}:$$PATH MACOSX_DEPLOYMENT_TARGET=${MACOS_VERSION_MIN} \
	    sh ./Configure -S > configure.log 2>&1 && \
	ln -sf host/generate_uudmap generate_uudmap || exit 1

${.OBJDIR}/stage/usr/bin/perl: ${SRC}/config.h
	cd ${SRC} && export PATH=${BIN}:$$PATH MACOSX_DEPLOYMENT_TARGET=${MACOS_VERSION_MIN} && \
	${GMAKE} ${JOBS} > build.log 2>&1 && \
	${GMAKE} install DESTDIR=${.OBJDIR}/stage > install.log 2>&1 || \
	    { echo "perl: see ${SRC}/build.log and install.log" >&2; exit 1; }
	chmod u+w ${.OBJDIR}/stage${CORE}/libperl.dylib
	${TC_BIN}/install_name_tool -id ${CORE}/libperl.dylib ${.OBJDIR}/stage${CORE}/libperl.dylib
	for f in ${.OBJDIR}/stage/usr/bin/perl${PERL_VERSION} ${.OBJDIR}/stage/usr/bin/perl; do \
	    [ -L $$f ] || { chmod u+w $$f && ${TC_BIN}/install_name_tool \
	        -change ${SRC}/libperl.dylib ${CORE}/libperl.dylib $$f; } || exit 1; done
	for f in Config.pm Config_heavy.pl; do \
	    c=${.OBJDIR}/stage/usr/lib/perl5/${PERL_VERSION}/${ARCH}/$$f; chmod u+w $$c && \
	    sed -i -e 's|--sysroot=${SDK} *||g' -e 's|${TC_BIN}/\.\./lib/clang/[0-9]*/[a-z]* *||g' \
	        -e 's|${TC_BIN}/llvm-|/usr/bin/|g' -e 's|${SDK}||g' -e 's|minkar-cc|cc|g' \
	        -e "s|^usecrosscompile='define'|usecrosscompile='undef'|" \
	        $$c && \
	    sed -i -E -e "s#^(hostperl|hostgenerate|run|to|from|targetarch|targethost|targetdir|targetmkdir|targetport|targetuser|targetenv|targetsh|config_arg[0-9]*|config_args|src)='.*'#\1=''#" \
	        -e 's#${SRC}#.#g' $$c || exit 1; done
	if grep -rlI -e '${SRC}' -e '${SDK}' ${.OBJDIR}/stage/usr/lib/perl5/${PERL_VERSION}/${ARCH}/Config*.p?; then \
	    echo "perl: build paths left in Config" >&2; exit 1; fi

${.OBJDIR}/.installed: ${.OBJDIR}/stage/usr/bin/perl
	rsync -rlc ${.OBJDIR}/stage/usr/bin/ ${SYSROOT_DIR}/usr/bin/
	rsync -rlc --exclude .packlist ${.OBJDIR}/stage/usr/lib/perl5/ ${SYSROOT_DIR}/usr/lib/perl5/
	rsync -rlc ${.OBJDIR}/stage/usr/share/man/ ${SYSROOT_DIR}/usr/share/man/
	mkdir -p ${SYSROOT_DIR}/usr/share/licenses/perl
	install -C -m 0444 ${SRC}/Artistic ${SRC}/Copying ${SYSROOT_DIR}/usr/share/licenses/perl/
	touch ${.TARGET}
