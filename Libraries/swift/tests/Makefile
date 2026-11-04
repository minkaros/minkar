# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# swift-hello, built with the toolchain's swiftc against the runtime of
# ../ (Libraries/swift): /usr/local/bin/swift-hello, which boottest's
# M3-swift gate runs.

SUBDIR =
TC =		${TOOLCHAIN}/usr/bin
PROG_SWIFT =	swift-hello

all: ${PROG_SWIFT}

# The runtime is the system's (6.0, in /usr/lib/swift): no back-deployment
# compatibility libraries.
${PROG_SWIFT}: ${.CURDIR}/swift-hello.swift
	env -u MACOSX_DEPLOYMENT_TARGET ${TC}/swiftc -frontend -c -O \
	    -runtime-compatibility-version none -target x86_64-apple-macosx10.15 \
	    -sdk ${MINKAR_SDKROOT} -module-name main ${.ALLSRC} -o ${.TARGET}.o
	env -u MACOSX_DEPLOYMENT_TARGET ${TC}/clang -target x86_64-apple-macosx10.15 \
	    -isysroot ${MINKAR_SDKROOT} -fuse-ld=${TC}/ld64 -Wno-incompatible-sysroot \
	    ${.TARGET}.o -L${MINKAR_SDKROOT}/usr/lib/swift -lswiftCore \
	    -Wl,-rpath,/usr/lib/swift -o ${.TARGET}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0755 ${.TARGET} ${SYSROOT_DIR}/usr/local/bin/${PROG_SWIFT}

clean: .PHONY
	rm -f ${PROG_SWIFT} ${PROG_SWIFT}.o

.include <bsd.prog.mk>
