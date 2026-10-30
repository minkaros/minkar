# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ctfconvert, ctfmerge and ctfdump for the build machine (xnu's build makes
# the kernel's CTF with them, for dtrace), from the same sources as
# /usr/sbin/dtrace: Apple's dtrace-338.100.1 in BSD/usr.sbin/dtrace/dist
# (its libelf, libdwarf, libctf and tools/ctfconvert).
#
# They are Darwin programs: they build with __APPLE__ (their Mach-O code
# paths) and host/ gives what Darwin's headers and libSystem would
# (host_prelude.h, host_shim.c: BSD's qsort_r, dispatch semaphores on
# pthreads, fls). Their C++ uses LLVM's ADT from the toolchain's LLVM
# sources, with host/llvm-ADT standing for the dist's copy: its DenseMap
# keys C strings by content, as that copy does (host_llvm.cpp: the few
# LLVMSupport functions DenseMap calls). include/ holds the generated
# llvm/Config headers (and headers cctools uses).

DT =		${ROOT_SOURCE_DIR}/BSD/usr.sbin/dtrace/dist
HOST =		${.CURDIR}/host
CTF_INC =	-I${HOST} -I${DT}/compat/opensolaris/sys \
		-I${DT}/compat/opensolaris -I${DT}/lib/libelf -I${DT}/lib/libdwarf \
		-I${DT}/lib/libctf/common -I${DT}/tools/ctfconvert \
		-I${.CURDIR}/../include
CTF_DEFS =	-w -O2 -D__APPLE__ -D__APPLE_CPP__ -D__LITTLE_ENDIAN__ \
		-DDWARF_SIMPLE_MALLOC -DCTF_OLD_VERSIONS \
		-include ${HOST}/host_prelude.h
CTF_CFLAGS =	${CTF_DEFS} ${CTF_INC} -I${DT}/include
CTF_CXXFLAGS =	${CTF_DEFS} -D__unused= -std=c++17 ${CTF_INC} \
		-I${.CURDIR}/../llvm/llvm/include -I${.CURDIR}/include

ELF_SRCS !=	cd ${DT}/lib/libelf && ls *.c
DWARF_SRCS !=	cd ${DT}/lib/libdwarf && ls *.c | grep -v '^dwarf_addr_finder\.c$$'
LIBCTF_SRCS !=	cd ${DT}/lib/libctf/common && ls *.c
TOOL_SRCS =	alist.cpp array.c atom.cpp barrier.c ctf.c dwarf.c fifo.c hash.c \
		iidesc.c input.c list.c memory.c merge.c output.c stack.c \
		strtab.cpp symbol.c tdata.c traverse.c util.c

LIB_OBJS =
.for s in ${ELF_SRCS}
elf-${s:R}.o: ${DT}/lib/libelf/${s}
	${HOST_CC} ${CTF_CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
LIB_OBJS +=	elf-${s:R}.o
.endfor
.for s in ${DWARF_SRCS}
dwarf-${s:R}.o: ${DT}/lib/libdwarf/${s}
	${HOST_CC} ${CTF_CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
LIB_OBJS +=	dwarf-${s:R}.o
.endfor
.for s in ${LIBCTF_SRCS}
libctf-${s:R}.o: ${DT}/lib/libctf/common/${s}
	${HOST_CC} ${CTF_CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
LIB_OBJS +=	libctf-${s:R}.o
.endfor
.for s in ${TOOL_SRCS:M*.c} ctfconvert.c ctfmerge.c dump.c utils.c
t-${s:R}.o: ${DT}/tools/ctfconvert/${s}
	${HOST_CC} ${CTF_CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
.endfor
.for s in ${TOOL_SRCS:M*.cpp}
t-${s:R}.o: ${DT}/tools/ctfconvert/${s}
	${HOST_CXX} ${CTF_CXXFLAGS} -c ${.ALLSRC} -o ${.TARGET}
.endfor
host_shim.o: ${HOST}/host_shim.c
	${HOST_CC} -O2 -c ${.ALLSRC} -o ${.TARGET}
host_llvm.o: ${HOST}/host_llvm.cpp
	${HOST_CXX} -O2 -std=c++17 -c ${.ALLSRC} -o ${.TARGET}

COMMON_OBJS =	${TOOL_SRCS:R:S/^/t-/:S/$/.o/} ${LIB_OBJS} host_shim.o host_llvm.o

all: ${TOOLS}/ctfconvert ${TOOLS}/ctfmerge ${TOOLS}/ctfdump

${TOOLS}/ctfconvert: ${COMMON_OBJS} t-ctfconvert.o
	${HOST_CXX} -o ${.TARGET} ${.ALLSRC} -lz -lpthread
${TOOLS}/ctfmerge: ${COMMON_OBJS} t-ctfmerge.o t-utils.o
	${HOST_CXX} -o ${.TARGET} ${.ALLSRC} -lz -lpthread
${TOOLS}/ctfdump: ${COMMON_OBJS} t-dump.o t-utils.o
	${HOST_CXX} -o ${.TARGET} ${.ALLSRC} -lz -lpthread

clean:
	rm -f *.o
