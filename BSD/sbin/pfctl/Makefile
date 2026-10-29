# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# pfctl: OpenBSD 4.3's sbin/pfctl (BSD and ISC licences, each file's
# header), the release whose pf Darwin's kernel carries, with
# sys/net/pf_ruleset.c from the same release, and its pf.os (p0f's
# fingerprints, ISC). It talks to Darwin's pf through the kernel's private
# net/pfvar.h, copied with the headers it needs into ${.OBJDIR}/inc;
# pfctl_darwin.h gives Darwin's names for OpenBSD's (SIMPLEQ is STAILQ).
# Mechanical renames: pf_addr's v4 and v6 are v4addr and v6addr, a rule
# address's port and port_op are xport.range.port and .op. Darwin has no
# interface groups and one routing table; its kernel has no ALTQ (pfctl
# says so and leaves queues alone, as on macOS). pf.conf is ours. Other
# changes for Darwin are the lines marked Minkar.

SUBDIR =
MK_WERROR = no
WARNS ?= 1
PROG =	pfctl
SRCS =	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c \
	pfctl_osfp.c pfctl_radix.c pfctl_table.c pfctl_qstats.c \
	pfctl_optimize.c pf_ruleset.c
MAN =
YACC =		byacc
YFLAGS =
XNU =	${ROOT_SOURCE_DIR}/Kernel/xnu
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DPRIVATE -I${.OBJDIR}/inc -I${.CURDIR} \
	-include pfctl_darwin.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lm

${SRCS:R:S/$/.o/} ${SRCS:R:S/$/.pieo/}: ${.OBJDIR}/inc/net/pfvar.h

${.OBJDIR}/inc/net/pfvar.h: ${XNU}/bsd/net/pfvar.h
	mkdir -p ${.OBJDIR}/inc/net/altq ${.OBJDIR}/inc/net/pktsched ${.OBJDIR}/inc/net/classq ${.OBJDIR}/inc/libkern
	cp ${XNU}/bsd/net/pfvar.h ${XNU}/bsd/net/if_pflog.h ${XNU}/bsd/net/radix.h \
	    ${.OBJDIR}/inc/net/
	cp ${XNU}/bsd/net/altq/*.h ${.OBJDIR}/inc/net/altq/
	cp ${XNU}/bsd/net/pktsched/*.h ${.OBJDIR}/inc/net/pktsched/
	cp ${XNU}/bsd/net/classq/*.h ${.OBJDIR}/inc/net/classq/
	cp ${XNU}/libkern/libkern/tree.h ${.OBJDIR}/inc/libkern/

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/etc ${SYSROOT_DIR}/usr/share/man/man5 \
	    ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/pfctl
	install -m 0444 ${.CURDIR}/pf.os ${SYSROOT_DIR}/etc/pf.os
	install -m 0644 ${.CURDIR}/pf.conf ${SYSROOT_DIR}/etc/pf.conf
	install -m 0444 ${.CURDIR}/pfctl.8 ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0444 ${.CURDIR}/pf.conf.5 ${.CURDIR}/pf.os.5 ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>
