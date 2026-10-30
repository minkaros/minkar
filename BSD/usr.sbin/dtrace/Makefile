# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# dtrace: Apple's dtrace-338.100.1 (macOS 10.15.6; CDDL, with some APSL
# files, dist/APPLE_LICENSE and each file's header), whose kernel half
# xnu 19.6 carries. dist/ is the tag's tree (github.com/apple-oss-
# distributions/dtrace, tarball sha256
# d39f9d77a4851d12916e2a2475ae29d6d54e9419a4ceed67ad54d6c7866d8d3f)
# without its tests and Xcode project. Built without CoreSymbolication (as Apple's dtrace_host), so
# for now it symbolicates neither processes nor kexts, and with no CTF
# in the kernel, D's kernel types (user_addr_t, darwin.d's) are missing:
# the syscall, mach_trap, profile and lockstat providers and D's own
# types work. dtrace_minkar.h gives what macOS's headers bring along. libelf, libctf, libproc,
# libdtrace and the command are linked into one program. Changes for
# Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
PROG_CXX =	dtrace
DIST =		${.CURDIR}/dist
XNU =		${ROOT_SOURCE_DIR}/Kernel/xnu
.PATH: ${DIST}/cmd/dtrace ${DIST}/lib/libelf ${DIST}/lib/libctf/common \
	${DIST}/lib/libproc ${DIST}/lib/libdtrace/common \
	${DIST}/lib/libdtrace/apple ${DIST}/lib/libdtrace/i386 \
	${DIST}/compat/opensolaris
SRCS =	dtrace.c
SRCS +=	begin.c clscook.c clscook_ELF64.c cntl.c cook.c data.c end.c error.c \
	gelf.c getdata.c getehdr.c getident.c getscn.c getshdr.c \
	getshstrndx.c input.c kind.c ndxscn.c nextscn.c strptr.c xlate64.c \
	xlate.c
SRCS +=	ctf_create.c ctf_decl.c ctf_error.c ctf_hash.c ctf_labels.c ctf_lib.c \
	ctf_lookup.c ctf_open.c ctf_subr.c ctf_types.c ctf_util.c
SRCS +=	libproc.c
SRCS +=	dt_grammar.y dt_lex.l dt_aggregate.c dt_as.c dt_buf.c dt_cc.c dt_cg.c \
	dt_consume.c dt_decl.c dt_dis.c dt_dof.c dt_error.c dt_errtags.c \
	dt_handle.c dt_ident.c dt_inttab.c dt_list.c dt_map.c dt_module.c \
	dt_names.c dt_open.c dt_options.c dt_parser.c dt_pcb.c dt_pid.c \
	dt_pq.c dt_pragma.c dt_print.c dt_printf.c dt_proc.c dt_program.c \
	dt_provider.c dt_regset.c dt_string.c dt_strtab.c dt_subr.c \
	dt_sugar.c dt_work.c dt_xlator.c
SRCS +=	dt_ld.cpp dt_module_apple.c dt_pid_apple.c dt_proc_apple.c \
	dt_provider_apple.c dt_subr_apple.c
SRCS +=	dis_tables.c dt_isadep.c darwin_shim.c
MAN =
YFLAGS =	-d -t	# -t: yydebug, which dt_cc.c sets
LFLAGS =	-l	# lex's yytext[], which dt_impl.h declares
LIBSYSTEM_PRIVATE =	${ROOT_SOURCE_DIR}/Libraries/Libsystem/private
DT_INC =	-I${.OBJDIR} -I${.OBJDIR}/inc -I${DIST}/compat/opensolaris \
	-I${DIST}/compat/opensolaris/sys \
	-I${DIST}/include -I${DIST}/lib/libelf -I${DIST}/lib/libctf/common \
	-I${DIST}/lib/libproc -I${DIST}/lib/libdtrace/common \
	-I${DIST}/lib/libdtrace/apple -I${DIST}/lib/libdtrace/i386
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DPRIVATE -DCTF_OLD_VERSIONS -DMINKAR_NO_OS_LOG ${DT_INC} \
	-idirafter ${XNU}/libkern -idirafter ${LIBSYSTEM_PRIVATE} \
	-include ${.CURDIR}/dtrace_minkar.h
CXXFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DPRIVATE -DMINKAR_NO_OS_LOG ${DT_INC} -std=c++11 \
	-idirafter ${XNU}/libkern -include ${.CURDIR}/dtrace_minkar.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation -framework IOKit

${SRCS:M*.c:R:S/$/.o/} ${SRCS:M*.c:R:S/$/.pieo/} dt_ld.o dt_ld.pieo \
dt_grammar.o dt_grammar.pieo dt_lex.o dt_lex.pieo: ${.OBJDIR}/inc/sys/dtrace.h

# The kernel's private headers dtrace uses.
${.OBJDIR}/inc/sys/dtrace.h: ${XNU}/bsd/sys/dtrace.h
	mkdir -p ${.OBJDIR}/inc/sys
	cd ${XNU}/bsd/sys && cp dtrace.h dtrace_glue.h fasttrap.h fasttrap_isa.h \
	    lockstat.h csr.h codesign.h kas_info.h proc_info.h event.h \
	    ${.OBJDIR}/inc/sys/

# The D libraries that need no kernel types: darwin.d (and io.d, ip.d,
# sched.d, socket.d, tcp.d, which depend on it) needs the kernel's CTF,
# which Minkar's kernel does not carry yet.
DLIBS =	errno.d regs_x86_64.d signal.d unistd.d

all: to-sysroot

to-sysroot: ${PROG_CXX}
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/lib/dtrace \
	    ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 -o 0 -g 0 ${PROG_CXX} ${SYSROOT_DIR}/usr/sbin/dtrace
	cd ${XNU}/bsd/dev/dtrace/scripts && install -m 0444 ${DLIBS} \
	    ${SYSROOT_DIR}/usr/lib/dtrace/
	install -m 0444 ${DIST}/cmd/dtrace/dtrace.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
