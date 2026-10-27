SUBDIR =
PROG=	ed
SRCS=	buf.c glbl.c io.c main.c re.c sub.c undo.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/

.include <bsd.prog.mk>
