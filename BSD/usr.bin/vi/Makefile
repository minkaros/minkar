# vi: nvi (Keith Bostic, BSD: LICENSE), from FreeBSD's contrib/nvi in git
# history, with FreeBSD's config.h and generated headers. ex, view, nvi,
# nex and nview are links. Built without iconv (no file encoding
# conversion).

SUBDIR =
MK_WERROR = no
MAN =
PROG =	nvi
NVI =	${.CURDIR}/nvi
.PATH: ${NVI}/common ${NVI}/ex ${NVI}/cl ${NVI}/vi ${NVI}/regex

SRCS =	cl_funcs.c cl_main.c cl_read.c cl_screen.c cl_term.c conv.c cut.c delete.c \
	encoding.c exf.c key.c line.c log.c main.c mark.c msg.c \
	options.c options_f.c put.c recover.c screen.c search.c seq.c util.c \
	ex.c ex_abbrev.c ex_append.c ex_args.c ex_argv.c ex_at.c ex_bang.c ex_cd.c \
	ex_cmd.c ex_cscope.c ex_delete.c ex_display.c ex_edit.c ex_equal.c ex_file.c ex_filter.c \
	ex_global.c ex_init.c ex_join.c ex_map.c ex_mark.c ex_mkexrc.c ex_move.c ex_open.c \
	ex_preserve.c ex_print.c ex_put.c ex_quit.c ex_read.c ex_screen.c ex_script.c ex_set.c \
	ex_shell.c ex_shift.c ex_source.c ex_stop.c ex_subst.c ex_tag.c ex_txt.c ex_undo.c \
	ex_usage.c ex_util.c ex_version.c ex_visual.c ex_write.c ex_yank.c ex_z.c getc.c \
	v_at.c v_ch.c v_cmd.c v_delete.c v_ex.c v_increment.c v_init.c v_itxt.c \
	v_left.c v_mark.c v_match.c v_paragraph.c v_put.c v_redraw.c v_replace.c v_right.c \
	v_screen.c v_scroll.c v_search.c v_section.c v_sentence.c v_status.c v_txt.c v_ulcase.c \
	v_undo.c v_util.c v_word.c v_xchar.c v_yank.c v_z.c v_zexit.c vi.c \
	vs_line.c vs_msg.c vs_refresh.c vs_relative.c vs_smap.c vs_split.c regcomp.c regerror.c \
	regexec.c regfree.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -D__REGEX_PRIVATE -D_XOPEN_SOURCE_EXTENDED=1 \
	-I${.CURDIR} -I${NVI} -I${NVI}/regex -I${.CURDIR}/ex \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -lncurses -lresolv \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
.for l in vi view ex nex nview
	ln -f ${SYSROOT_DIR}/usr/bin/nvi ${SYSROOT_DIR}/usr/bin/${l}
.endfor

.include <bsd.prog.mk>
