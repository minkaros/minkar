SUBDIR =
PROG=	pax
SRCS=	ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c ftree.c gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c tar.c tty_subs.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -include signal.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/

.include <bsd.prog.mk>
