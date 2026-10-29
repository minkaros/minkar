# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libuv 1.53.0 (MIT, dist/LICENSE; libuv.org), the event loop BIND 9.20
# needs: dist/ holds upstream's libuv-v1.53.0.tar.gz (sha256
# cb0d6dd2128d5a95bd242c6cc982a24fe608fa93da57b6b4ec763b0018c53e64,
# signature checked against Santiago Gimeno's key) without test/, docs/
# and img/. Built with its CMake as a static library, kept with its
# headers in ${.OBJDIR}/stage for BIND; nothing goes into the image.

all: ${.OBJDIR}/stage/lib/libuv.a

TC_BIN =	${TOOLCHAIN}/usr/bin
UV_FLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Wno-nullability-completeness

${.OBJDIR}/stage/lib/libuv.a: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/build ${.OBJDIR}/stage
	env -u CFLAGS -u LDFLAGS cmake -G Ninja -S ${.CURDIR}/dist \
	    -B ${.OBJDIR}/build -Wno-dev -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_SYSTEM_PROCESSOR=${CpuArch} \
	    -DCMAKE_C_COMPILER=${CC} -DCMAKE_AR=${TC_BIN}/llvm-ar \
	    -DCMAKE_RANLIB=${TC_BIN}/llvm-ranlib \
	    -DCMAKE_OSX_SYSROOT=${MINKAR_SDKROOT} \
	    -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_VERSION_MIN} \
	    "-DCMAKE_C_FLAGS=${UV_FLAGS}" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
	    -DLIBUV_BUILD_SHARED=OFF -DLIBUV_BUILD_TESTS=OFF \
	    -DLIBUV_BUILD_BENCH=OFF -DBUILD_TESTING=OFF \
	    -DCMAKE_INSTALL_PREFIX=${.OBJDIR}/stage -DCMAKE_INSTALL_LIBDIR=lib
	env -u CFLAGS -u LDFLAGS ninja -C ${.OBJDIR}/build -j${.MAKE.JOBS:U1} install
