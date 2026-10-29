# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ar, size and strings: LLVM's llvm-ar, llvm-size and llvm-strings
# (Apache-2.0 with the LLVM exception), from the llvm-project already in
# Developer/Default.xctoolchain/llvm (the one our compiler is built from),
# cross-built for Darwin with CMake and Ninja; llvm-tblgen is the
# toolchain's. They read and write Mach-O: ar makes Darwin's archive
# format with its symbol table (ranlib is ar under that name). Only the
# X86 target, and none of the optional libraries (zlib, zstd, terminfo,
# libxml2). Installs ar, ranlib, size and strings in /usr/bin, as on
# macOS, with our manual pages.

all: ${.OBJDIR}/.installed

LLVM_SRC =	${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/llvm/llvm
TC_BIN =	${TOOLCHAIN}/usr/bin
LLVM_FLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Wno-nullability-completeness

${.OBJDIR}/build/build.ninja: ${.CURDIR}/Makefile
	env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -G Ninja -S ${LLVM_SRC} \
	    -B ${.OBJDIR}/build -Wno-dev \
	    -DCMAKE_BUILD_TYPE=MinSizeRel \
	    -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_SYSTEM_PROCESSOR=${CpuArch} \
	    -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} \
	    -DCMAKE_AR=${TC_BIN}/llvm-ar -DCMAKE_RANLIB=${TC_BIN}/llvm-ranlib \
	    -DCMAKE_LIBTOOL=${TC_BIN}/llvm-libtool-darwin \
	    -DCMAKE_OSX_SYSROOT=${MINKAR_SDKROOT} \
	    -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_VERSION_MIN} \
	    "-DCMAKE_C_FLAGS=${LLVM_FLAGS}" "-DCMAKE_CXX_FLAGS=${LLVM_FLAGS}" \
	    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=${LD} -Wl,-dead_strip" \
	    -DLLVM_HOST_TRIPLE=${CpuArch}-apple-darwin19 \
	    -DLLVM_DEFAULT_TARGET_TRIPLE=${CpuArch}-apple-darwin \
	    -DLLVM_TARGETS_TO_BUILD=X86 \
	    -DLLVM_TABLEGEN=${TC_BIN}/llvm-tblgen \
	    -DLLVM_NATIVE_TOOL_DIR=${TC_BIN} \
	    -DLLVM_ENABLE_PROJECTS= -DLLVM_ENABLE_RUNTIMES= \
	    -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF \
	    -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_LIBXML2=OFF \
	    -DLLVM_ENABLE_LIBEDIT=OFF -DLLVM_ENABLE_LIBPFM=OFF \
	    -DLLVM_ENABLE_BINDINGS=OFF -DLLVM_ENABLE_WARNINGS=OFF \
	    -DLLVM_APPEND_VC_REV=OFF -DHAVE_CRASHREPORTERCLIENT_H=0 \
	    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF \
	    -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_DOCS=OFF \
	    -DLLVM_INCLUDE_UTILS=OFF -DLLVM_BUILD_UTILS=OFF

${.OBJDIR}/build/bin/llvm-ar: ${.OBJDIR}/build/build.ninja
	env -u CFLAGS -u CXXFLAGS -u LDFLAGS ninja -C ${.OBJDIR}/build \
	    -j${.MAKE.JOBS:U1} llvm-ar llvm-size llvm-strings

${.OBJDIR}/.installed: ${.OBJDIR}/build/bin/llvm-ar
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/build/bin/llvm-ar ${SYSROOT_DIR}/usr/bin/ar
	ln -sf ar ${SYSROOT_DIR}/usr/bin/ranlib
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/build/bin/llvm-size ${SYSROOT_DIR}/usr/bin/size
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/build/bin/llvm-strings ${SYSROOT_DIR}/usr/bin/strings
.for m in ar ranlib size strings
	install -m 0444 ${.CURDIR}/${m}.1 ${SYSROOT_DIR}/usr/share/man/man1/${m}.1
.endfor
	touch ${.TARGET}
