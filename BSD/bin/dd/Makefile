SUBDIR =
PROG=	dd
SRCS=	args.c conv.c conv_tab.c dd.c misc.c position.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libutil -include time.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/

.include <bsd.prog.mk>
