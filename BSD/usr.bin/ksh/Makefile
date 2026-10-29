# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ksh 93u+m/1.0.10 (EPL-2.0, dist/LICENSE.md; github.com/ksh93/ksh), the
# KornShell macOS ships as /bin/ksh. dist/ is upstream's tag v1.0.10
# (archive sha256 9f4c7a9531cec6941d6a9fd7fb70a4aeda24ea32800f578fd4099083f98b4e8a)
# without its CI files, unchanged.
#
# ksh builds with AT&T's package/mamake/iffe system, which runs thousands of
# small test programs to configure itself and does not cross-compile on its
# own. It is built twice in a copy of dist/: for this machine first (its
# tools: mamake, iffe, lcgen), then for Darwin, where iffe runs each probe
# through crossexec. kx-cp and kx-sh stand in for crossexec's scp and ssh:
# they answer from iffe-results, the probes' outputs recorded on Minkar, so
# no Minkar machine is needed. `make record` (with the dev VM up, see
# minkar-tests' devvm.sh) runs the probes on it and rewrites iffe-results;
# a probe with no recorded answer stops the build. IFFEFLAGS is in the
# environment too: ksh93's features/math.sh runs iffe itself.
#
# In the copy, the Mamfiles call the toolchain's ar (Darwin format) and
# ranlib (the host's corrupts Mach-O archives), and lcgen, which writes
# platform-independent tables, is the host's.

all: ${.OBJDIR}/.installed

TC =		${TOOLS}
KX_PORT ?=	2244
KX_KEY ?=	/work/build/boottest-ssh/id_ed25519
B =		${.OBJDIR}/build
X =		${.OBJDIR}/xbin

KSH_ENV =	HOME=${.OBJDIR}/home KX_STATE=${.OBJDIR}/kx \
		KX_DB=${.CURDIR}/iffe-results KX_PORT=${KX_PORT} KX_KEY=${KX_KEY} \
		KX_RECORD=${KX_RECORD} IFFEFLAGS="-x darwin.i386-64" \
		DEFPATH=/bin:/usr/bin PATH=${X}:${B}/arch/linux.i386-64/bin:$${PATH}

${.OBJDIR}/ksh: ${.CURDIR}/Makefile
	rm -rf ${B} ${X} ${.OBJDIR}/home ${.OBJDIR}/kx
	cp -R ${.CURDIR}/dist ${B}
	mkdir -p ${X} ${.OBJDIR}/home ${.OBJDIR}/kx
	printf '#!/bin/sh\nexec %s --sysroot=%s -mmacos-version-min=%s -arch x86_64 --ld-path=%s -Wno-nullability-completeness "$$@"\n' \
	    ${CC} ${MINKAR_SDKROOT} ${MACOS_VERSION_MIN} ${LD} > ${X}/cc-minkar
	printf '#!/bin/sh\nexec %s/llvm-ar --format=darwin "$$@"\n' ${TC} > ${X}/ar-minkar
	chmod +x ${X}/cc-minkar ${X}/ar-minkar
	printf 'darwin.i386-64\t127.0.0.1\troot\t/tmp\t%s\t%s\n' \
	    ${.CURDIR}/kx-sh ${.CURDIR}/kx-cp > ${.OBJDIR}/home/.crossexec
	# This machine's tools, then the Darwin build that uses them.
	cd ${B} && env -u MAKEFLAGS DEFPATH=/bin:/usr/bin bin/package make CC=cc
	cp ${B}/bin/package ${B}/arch/linux.i386-64/bin/package
	# The Darwin build's Mamfiles (see above).
	cd ${B}/src && for m in lib/libast lib/libdll lib/libsum lib/libcmd cmd/ksh93; do \
	    sed -i -e 's|exec - ranlib %{@}|exec - ${TC}/llvm-ranlib %{@}|' \
		-e 's|^setv AR %{mam_cc_AR}|setv AR ${X}/ar-minkar|' $$m/Mamfile; done
	sed -i 's|exec - lcgen lc.h lctab.c|exec - ${B}/arch/linux.i386-64/src/lib/libast/lcgen lc.h lctab.c|' \
	    ${B}/src/lib/libast/Mamfile
	cd ${B} && env -u MAKEFLAGS ${KSH_ENV} bin/package make \
	    HOSTTYPE=darwin.i386-64 CC=${X}/cc-minkar "IFFEFLAGS=-x darwin.i386-64" || true
	@if [ -s ${.OBJDIR}/kx/missing ]; then \
	    echo "ksh: probes with no recorded result (make record with the dev VM up):"; \
	    sort -u ${.OBJDIR}/kx/missing | head -20; exit 1; fi
	cp ${B}/arch/darwin.i386-64/bin/ksh ${.TARGET}

record: .PHONY
	rm -f ${.CURDIR}/iffe-results ${.OBJDIR}/ksh
	${MAKE} -f ${.CURDIR}/Makefile KX_RECORD=1 ${.OBJDIR}/ksh

${.OBJDIR}/.installed: ${.OBJDIR}/ksh
	mkdir -p ${SYSROOT_DIR}/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/ksh ${SYSROOT_DIR}/bin/ksh
	install -m 0444 ${B}/src/cmd/ksh93/sh.1 ${SYSROOT_DIR}/usr/share/man/man1/ksh.1
	touch ${.TARGET}
