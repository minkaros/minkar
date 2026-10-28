SUBDIR =
PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
MAN =
MK_WERROR = no
CFLAGS += -DCOLORLS -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lncurses

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/ls

.include <bsd.prog.mk>
