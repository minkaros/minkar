# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# mandoc: FreeBSD main's contrib/mandoc (mandoc 1.14, ISC licence; see
# LICENSE), the manual formatter man(1) runs, with apropos, whatis and
# makewhatis (the same program, by name). config.h is Minkar's: what
# mandoc's configure finds in Minkar's SDK. The functions Darwin's libc
# lacks come from mandoc's own compat_*.c.
# As on macOS, makewhatis lives in /usr/libexec.

SUBDIR =
MK_WERROR = no
PROG =	mandoc
MAN =

LIB_SRCS = man.c man_macro.c man_validate.c \
	arch.c att.c lib.c mdoc.c mdoc_argv.c mdoc_macro.c mdoc_markdown.c \
	mdoc_state.c mdoc_validate.c st.c \
	eqn.c roff.c roff_escape.c roff_html.c roff_term.c roff_validate.c \
	tbl.c tbl_data.c tbl_layout.c tbl_opts.c \
	chars.c mandoc.c mandoc_aux.c mandoc_dbg.c mandoc_msg.c mandoc_ohash.c \
	mandoc_xr.c msec.c preconv.c read.c
HTML_SRCS = eqn_html.c html.c man_html.c mdoc_html.c tbl_html.c
TERM_SRCS = eqn_term.c man_term.c mdoc_term.c term.c term_ascii.c \
	term_ps.c term_tab.c term_tag.c tbl_term.c
DB_SRCS = dbm.c dbm_map.c mansearch.c \
	dba.c dba_array.c dba_read.c dba_write.c mandocdb.c
COMPAT_SRCS = compat_ohash.c compat_reallocarray.c compat_recallocarray.c \
	compat_strtonum.c
SRCS =	${LIB_SRCS} ${HTML_SRCS} mdoc_man.c ${TERM_SRCS} ${DB_SRCS} \
	${COMPAT_SRCS} main.c manpath.c out.c tag.c tree.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DHAVE_CONFIG_H -I${.CURDIR}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lz

MAN1 =	mandoc.1 apropos.1
MAN5 =	mandoc.db.5
MAN7 =	eqn.7 mandoc_char.7 tbl.7 man.7 mdoc.7 roff.7
MAN8 =	makewhatis.8

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/libexec
.for d in 1 5 7 8
	mkdir -p ${SYSROOT_DIR}/usr/share/man/man${d}
.endfor
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/mandoc
	ln -f ${SYSROOT_DIR}/usr/bin/mandoc ${SYSROOT_DIR}/usr/bin/apropos
	ln -f ${SYSROOT_DIR}/usr/bin/mandoc ${SYSROOT_DIR}/usr/bin/whatis
	ln -f ${SYSROOT_DIR}/usr/bin/mandoc ${SYSROOT_DIR}/usr/libexec/makewhatis
.for d in 1 5 7 8
.for m in ${MAN${d}}
	install -m 0444 ${.CURDIR}/${m} ${SYSROOT_DIR}/usr/share/man/man${d}/
.endfor
.endfor
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/apropos.1 \
	    ${SYSROOT_DIR}/usr/share/man/man1/whatis.1

.include <bsd.prog.mk>
