SUBDIR =
PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c printf.c \
	getdate.y
YFLAGS=
MAN =
MK_WERROR = no
NO_WMISSING_VARIABLE_DECLARATIONS=
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

# macOS has find in /usr/bin.
to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/find

.include <bsd.prog.mk>
