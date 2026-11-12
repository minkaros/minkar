CC := ${HOST_CC}
CXX := ${HOST_CXX}

all: install-llvm

.if "${.MAKE.OS}" != "Darwin"
CFLAGS = -UHAVE_MACH_MACH_H -UAPPLE -U__APPLE__ -UHAVE_DECL_ARC4RANDOM
CXXFLAGS = ${CFLAGS} -U_GLIBCXX_USE_CXX11_ABI -D_GLIBCXX_USE_CXX11_ABI=0
.endif

# Built again only when what it is built from changes, by content: the git
# tree of these sources (and any uncommitted change in them) and this
# Makefile; not by file dates, which a new checkout or another component's
# install changes. Outside git it is always built (its build tree makes
# that quick when nothing changed). Libraries also use the build tree
# (llvm_host's cmake packages), so it must be there too.
LLVM_HOST =	${.OBJDIR}/../llvm_host
STAMP =		${TOOLCHAIN}/usr/share/llvm/.minkar-built
GIT =		git -c safe.directory=* -C ${.CURDIR}
SUM_CMD =	{ ${GIT} rev-parse HEAD:./ && ${GIT} status --porcelain -- . && \
		    cat ${.CURDIR}/Makefile; } 2>/dev/null | cksum

install-llvm: .PHONY
	@if [ -x ${TOOLS}/clang ] && [ -f ${STAMP} ] && \
	    [ -f ${LLVM_HOST}/lib/cmake/llvm/LLVMConfig.cmake ] && \
	    ${GIT} rev-parse -q --verify HEAD:./ > /dev/null 2>&1 && \
	    [ "$$(${SUM_CMD})" = "$$(cat ${STAMP})" ]; then \
		echo "llvm: the toolchain's LLVM is current"; \
	else \
		${MAKE} -C ${.CURDIR} install-llvm-now; \
	fi

install-llvm-now: .PHONY
	cmake -S ${.CURDIR}/llvm -B ${LLVM_HOST} -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-C${.CURDIR}/clang/cmake/caches/Apple-stage1.cmake \
	-DCMAKE_C_COMPILER=${HOST_CC} \
	-DCMAKE_CXX_COMPILER=${HOST_CXX} \
	-DCMAKE_C_FLAGS=${CFLAGS} \
	"-DCMAKE_CXX_FLAGS=${CXXFLAGS}" \
	-DCMAKE_CXX_STANDARD=17 \
	-DCMAKE_CXX_STANDARD_REQUIRED=ON \
	-DCMAKE_INSTALL_PREFIX=${TOOLCHAIN}/usr \
	-DLLVM_BOOTSTRAP_DEFAULT_TARGET_TRIPLE=${CpuArch}-apple-darwin \
	-DLLVM_DEFAULT_TARGET_TRIPLE=${CpuArch}-apple-darwin \
	-DLLVM_TARGETS_TO_BUILD=${BuildArch} \
	-DLLVM_ENABLE_LTO=OFF \
	-DLLVM_APPEND_VC_REV=OFF \
	-DLLVM_ENABLE_PROJECTS="clang;tapi;lld" \
	-DLLVM_BUILD_RUNTIMES=ON \
	-DLLVM_BOOTSTRAP_ENABLE_LIBCXX=ON \
	-DLLVM_ENABLE_LIBCXX=OFF \
	-DLLVM_ENABLE_ZLIB=FORCE_ON \
	-DLLVM_BUILD_DOCS=OFF \
	-DLLVM_BUILD_TOOLS=OFF \
	-DLLVM_INCLUDE_TESTS=OFF \
	-DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_BUILD_EXAMPLES=OFF \
	-DLLVM_ENABLE_WARNINGS=OFF \
	-DLLVM_ENABLE_BINDINGS=OFF \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=ON \
	-DLLVM_INSTALL_BINUTILS_SYMLINKS=ON \
	-DLLVM_TOOL_LLVM_INSTALL_NAME_TOOL_BUILD=ON \
	-DCLANG_BUILD_TOOLS=ON \
	-DCLANG_BUILD_UTILS=ON \
	-DCLANG_ENABLE_STATIC_ANALYZER=OFF \
	-DCLANG_ENABLE_ARCMT=OFF \
	-DCLANG_ENABLE_FORMAT=OFF \
	-DCLANG_INCLUDE_TESTS=OFF \
	-DTAPI_INCLUDE_TESTS=OFF \
	-DTAPI_MINKAR_LIBRARY_ONLY=ON
	CC=${HOST_CC} CXX=${HOST_CXX} ${MAKE} -C${LLVM_HOST} all \
	   llvm-ar llvm-nm llvm-ranlib llvm-strip dsymutil llvm-lipo \
	   llvm-libtool-darwin llvm-objdump llvm-otool llvm-readtapi
	# By content: unchanged tools keep their dates.
	rsync -rlc ${LLVM_HOST}/bin ${TOOLCHAIN}/usr/
	rsync -rlc ${LLVM_HOST}/lib/clang ${TOOLCHAIN}/usr/lib/
	rsync -rlc ${LLVM_HOST}/lib/lib* ${TOOLCHAIN}/usr/lib/
.for tool in ar nm ranlib objdump libtool-darwin otool
	ln -sf llvm-${tool} ${TOOLCHAIN}/usr/bin/${tool}
.endfor
	ln -sf clang ${TOOLCHAIN}/usr/bin/cc
	ln -sf clang ${TOOLCHAIN}/usr/bin/c89
	ln -sf clang ${TOOLCHAIN}/usr/bin/c99
	ln -sf clang++ ${TOOLCHAIN}/usr/bin/c++
	mkdir -p ${TOOLCHAIN}/usr/share/llvm
	${SUM_CMD} > ${STAMP}
